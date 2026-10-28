#include <bmx_fba.H>

#include <AMReX.H>
#include <AMReX_Print.H>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef BMX_USE_LIBSBML
#include <sbml/SBMLDocument.h>
#include <sbml/SBMLReader.h>
#include <sbml/Model.h>
#include <sbml/Reaction.h>
#include <sbml/Species.h>
#include <sbml/SpeciesReference.h>
#include <sbml/Parameter.h>
#include <sbml/packages/fbc/extension/FbcExtension.h>
#include <sbml/packages/fbc/sbml/FbcModelPlugin.h>
#include <sbml/packages/fbc/sbml/FbcReactionPlugin.h>
#include <sbml/packages/fbc/sbml/FluxObjective.h>
#include <sbml/packages/fbc/sbml/Objective.h>
#endif

#ifdef BMX_USE_ORTOOLS
#include "ortools/linear_solver/linear_solver.h"
#endif

namespace BMXFBA
{
namespace
{
#ifdef BMX_USE_LIBSBML
  using namespace libsbml;
#endif

  struct ReactionData
  {
    std::string id;
    std::vector<std::pair<int, double>> stoichiometry;
    double lower_bound = -std::numeric_limits<double>::infinity();
    double upper_bound = std::numeric_limits<double>::infinity();
    double objective_coefficient = 0.0;
  };

  struct FbaCache
  {
    std::string model_path;
    std::vector<std::string> metabolite_ids;
    std::unordered_map<std::string, int> metabolite_index;
    std::vector<ReactionData> reactions;
    std::vector<double> objective_coefficients;
    bool objective_is_maximize = true;
    bool ready = false;
  };

  FbaCache cache;

#ifdef BMX_USE_LIBSBML
  double
  parameter_value_or_default(const Model* model, const std::string& id, double fallback)
  {
    if (!model || id.empty()) {
      return fallback;
    }

    const Parameter* parameter = model->getParameter(id);
    if (!parameter) {
      return fallback;
    }

    if (parameter->isSetValue()) {
      return parameter->getValue();
    }

    return fallback;
  }

  double
  flux_bound_value(const Model* model, const FbcReactionPlugin* reaction_plugin,
                   bool lower_bound, double fallback)
  {
    if (!model || !reaction_plugin) {
      return fallback;
    }

    const std::string bound_id = lower_bound ? reaction_plugin->getLowerFluxBound()
                                             : reaction_plugin->getUpperFluxBound();
    if (bound_id.empty()) {
      return fallback;
    }

    return parameter_value_or_default(model, bound_id, fallback);
  }

  void
  load_cache_from_model(const Model* model)
  {
    cache.metabolite_ids.clear();
    cache.metabolite_index.clear();
    cache.reactions.clear();
    cache.objective_coefficients.clear();
    cache.objective_is_maximize = true;

    const unsigned int num_species = model->getNumSpecies();
    cache.metabolite_ids.reserve(num_species);
    for (unsigned int i = 0; i < num_species; ++i) {
      const Species* species = model->getSpecies(i);
      if (!species) {
        continue;
      }

      const std::string species_id = species->getId();
      if (species_id.empty()) {
        continue;
      }

      cache.metabolite_index.emplace(species_id, static_cast<int>(cache.metabolite_ids.size()));
      cache.metabolite_ids.push_back(species_id);
    }

    const unsigned int num_reactions = model->getNumReactions();
    cache.reactions.reserve(num_reactions);
    cache.objective_coefficients.assign(num_reactions, 0.0);

    for (unsigned int i = 0; i < num_reactions; ++i) {
      const Reaction* reaction = model->getReaction(i);
      if (!reaction) {
        continue;
      }

      ReactionData data;
      data.id = reaction->getId();

      const auto* reaction_plugin = dynamic_cast<const FbcReactionPlugin*>(reaction->getPlugin("fbc"));
      const double default_lower = reaction->getReversible() ? -1000.0 : 0.0;
      const double default_upper = 1000.0;
      data.lower_bound = flux_bound_value(model, reaction_plugin, true, default_lower);
      data.upper_bound = flux_bound_value(model, reaction_plugin, false, default_upper);

      const unsigned int num_reactants = reaction->getNumReactants();
      for (unsigned int j = 0; j < num_reactants; ++j) {
        const SpeciesReference* species_reference = reaction->getReactant(j);
        if (!species_reference) {
          continue;
        }

        const auto found = cache.metabolite_index.find(species_reference->getSpecies());
        if (found == cache.metabolite_index.end()) {
          continue;
        }

        data.stoichiometry.emplace_back(found->second, -species_reference->getStoichiometry());
      }

      const unsigned int num_products = reaction->getNumProducts();
      for (unsigned int j = 0; j < num_products; ++j) {
        const SpeciesReference* species_reference = reaction->getProduct(j);
        if (!species_reference) {
          continue;
        }

        const auto found = cache.metabolite_index.find(species_reference->getSpecies());
        if (found == cache.metabolite_index.end()) {
          continue;
        }

        data.stoichiometry.emplace_back(found->second, species_reference->getStoichiometry());
      }

      cache.reactions.push_back(std::move(data));
    }

    struct ExchangeOverride
    {
      const char* reaction_id;
      double uptake_limit;
    };

    // Default medium for the fungi FBA example.
    // Edit this table if you want to remap BMX’s A/B/C/D/E/F species to
    // different KEGG exchange reactions or change the uptake limits.
    static constexpr ExchangeOverride exchange_overrides[] = {
      {"R_Ex_C00031", 0.11},
      {"R_Ex_C00007", 1000.0},
      {"R_Ex_C01342", 1000.0},
      {"R_Ex_C00009", 1000.0},
      {"R_Ex_C00001", 1000.0},
      {"R_Ex_C00059", 1000.0},
      {"R_Ex_CHEBI29033", 1000.0},
    };

    for (const auto& override_entry : exchange_overrides) {
      const auto reaction_iter = std::find_if(
        cache.reactions.begin(), cache.reactions.end(),
        [&override_entry](const ReactionData& reaction_data) { return reaction_data.id == override_entry.reaction_id; });
      if (reaction_iter == cache.reactions.end()) {
        continue;
      }

      reaction_iter->lower_bound = std::max(reaction_iter->lower_bound, -override_entry.uptake_limit);
    }

    const auto* model_plugin = dynamic_cast<const FbcModelPlugin*>(model->getPlugin("fbc"));
    if (model_plugin) {
      const Objective* objective = nullptr;
      const std::string active_objective_id = model_plugin->getActiveObjective();
      if (!active_objective_id.empty()) {
        objective = model_plugin->getObjective(active_objective_id);
      }
      if (!objective && model_plugin->getNumObjectives() > 0) {
        objective = model_plugin->getObjective(0);
      }

      if (objective) {
        cache.objective_is_maximize = objective->getType() != OBJECTIVE_TYPE_MINIMIZE;
        const unsigned int num_flux_objectives = objective->getNumFluxObjectives();
        for (unsigned int i = 0; i < num_flux_objectives; ++i) {
          const FluxObjective* flux_objective = objective->getFluxObjective(i);
          if (!flux_objective) {
            continue;
          }

          const std::string& reaction_id = flux_objective->getReaction();
          const auto reaction_iter = std::find_if(
            cache.reactions.begin(), cache.reactions.end(),
            [&reaction_id](const ReactionData& reaction_data) { return reaction_data.id == reaction_id; });
          if (reaction_iter == cache.reactions.end()) {
            continue;
          }

          const double coefficient = flux_objective->isSetCoefficient()
            ? flux_objective->getCoefficient()
            : 1.0;
          reaction_iter->objective_coefficient += coefficient;
          cache.objective_coefficients[static_cast<std::size_t>(reaction_iter - cache.reactions.begin())] += coefficient;
        }
      }
    }

    if (std::all_of(cache.objective_coefficients.begin(), cache.objective_coefficients.end(),
                    [](double value) { return value == 0.0; })) {
      const std::string biomass_reaction_id = "R_treebiomasspseudoreaction";
      const auto reaction_iter = std::find_if(
        cache.reactions.begin(), cache.reactions.end(),
        [&biomass_reaction_id](const ReactionData& reaction_data) { return reaction_data.id == biomass_reaction_id; });
      if (reaction_iter != cache.reactions.end()) {
        reaction_iter->objective_coefficient = 1.0;
        cache.objective_coefficients[static_cast<std::size_t>(reaction_iter - cache.reactions.begin())] = 1.0;
      }
    }

    cache.ready = true;
  }
#endif

#ifdef BMX_USE_ORTOOLS
  Real
  run_ortools_smoke_test()
  {
    amrex::Print() << "==================================================" << std::endl;
    amrex::Print() << "BMX OR-Tools smoke test: starting tiny LP solve" << std::endl;
    amrex::Print() << "==================================================" << std::endl;

    operations_research::MPSolver solver(
      "bmx_fba_smoke_test",
      operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING);

    const double infinity = solver.infinity();
    auto* x = solver.MakeNumVar(0.0, infinity, "x");
    auto* y = solver.MakeNumVar(0.0, infinity, "y");

    auto* c0 = solver.MakeRowConstraint(-infinity, 14.0, "c0");
    c0->SetCoefficient(x, 1.0);
    c0->SetCoefficient(y, 2.0);

    auto* c1 = solver.MakeRowConstraint(0.0, infinity, "c1");
    c1->SetCoefficient(x, 3.0);
    c1->SetCoefficient(y, -1.0);

    auto* c2 = solver.MakeRowConstraint(-infinity, 2.0, "c2");
    c2->SetCoefficient(x, 1.0);
    c2->SetCoefficient(y, -1.0);

    auto* objective = solver.MutableObjective();
    objective->SetCoefficient(x, 3.0);
    objective->SetCoefficient(y, 4.0);
    objective->SetMaximization();

    const auto status = solver.Solve();
    if (status != operations_research::MPSolver::OPTIMAL) {
      amrex::Abort("OR-Tools smoke LP failed to solve to optimality");
    }

    amrex::Print() << "BMX OR-Tools smoke test: solver status OPTIMAL" << std::endl;

    const Real objective_value = objective->Value();
    amrex::Print() << "BMX OR-Tools smoke test objective = "
                   << objective_value << std::endl;
    amrex::Print() << "==================================================" << std::endl;
    return objective_value;
  }
#endif
}

void
initialize_from_sbml(const std::string& sbml_file)
{
  cache.model_path = sbml_file;

  if (cache.model_path.empty()) {
    amrex::Abort("chem_species.fba_sbml_file must be set when use_fba_growth is enabled");
  }

#ifdef BMX_USE_LIBSBML
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(cache.model_path);
  if (!document) {
    amrex::Abort("Failed to create an SBML document from " + cache.model_path);
  }

  const auto* error_log = document->getErrorLog();
  const bool has_error_or_fatal = error_log &&
    (error_log->getNumFailsWithSeverity(LIBSBML_SEV_ERROR) > 0 ||
     error_log->getNumFailsWithSeverity(LIBSBML_SEV_FATAL) > 0);
  if (has_error_or_fatal) {
    document->printErrors();
    delete document;
    amrex::Abort("SBML document contains errors: " + cache.model_path);
  }

  if (document->getNumErrors() > 0) {
    document->printErrors();
  }

  if (document->getModel() == nullptr) {
    delete document;
    amrex::Abort("SBML document has no model: " + cache.model_path);
  }

  load_cache_from_model(document->getModel());

#ifdef BMX_USE_ORTOOLS
  (void)run_ortools_smoke_test();
#endif

  amrex::Print() << "Loaded FBA SBML model: " << cache.model_path << std::endl;
  delete document;
#else
  amrex::Abort("BMX was built without libSBML support but an SBML file was requested");
#endif
}

const std::string&
sbml_model_path()
{
  return cache.model_path;
}

Real
compute_fba_growth_rate_hr(const Real* /*cell_par*/, const Real* /*p_vals*/, const Real* /*fluid_vals*/,
                           Real /*volume_fraction*/, Real /*particle_count*/, int /*cell_type*/)
{
#ifndef BMX_USE_LIBSBML
  return 0.0;
#else
  if (!cache.ready) {
    amrex::Abort("FBA model cache is not initialized");
  }

#ifdef BMX_USE_ORTOOLS
  operations_research::MPSolver solver(
    "bmx_fba_growth_lp",
    operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING);

  const double infinity = solver.infinity();
  std::vector<operations_research::MPVariable*> flux_variables;
  flux_variables.reserve(cache.reactions.size());

  for (const auto& reaction : cache.reactions) {
    const double lower_bound = std::isfinite(reaction.lower_bound) ? reaction.lower_bound : -infinity;
    const double upper_bound = std::isfinite(reaction.upper_bound) ? reaction.upper_bound : infinity;
    flux_variables.push_back(solver.MakeNumVar(lower_bound, upper_bound, reaction.id));
  }

  for (std::size_t metabolite_index = 0; metabolite_index < cache.metabolite_ids.size(); ++metabolite_index) {
    auto* constraint = solver.MakeRowConstraint(0.0, 0.0, cache.metabolite_ids[metabolite_index]);
    for (std::size_t reaction_index = 0; reaction_index < cache.reactions.size(); ++reaction_index) {
      for (const auto& term : cache.reactions[reaction_index].stoichiometry) {
        if (static_cast<std::size_t>(term.first) == metabolite_index) {
          constraint->SetCoefficient(flux_variables[reaction_index], term.second);
        }
      }
    }
  }

  auto* objective = solver.MutableObjective();
  for (std::size_t reaction_index = 0; reaction_index < cache.reactions.size(); ++reaction_index) {
    const double coefficient = cache.reactions[reaction_index].objective_coefficient;
    if (coefficient != 0.0) {
      objective->SetCoefficient(flux_variables[reaction_index], coefficient);
    }
  }

  if (cache.objective_is_maximize) {
    objective->SetMaximization();
  } else {
    objective->SetMinimization();
  }

  const auto status = solver.Solve();
  if (status != operations_research::MPSolver::OPTIMAL) {
    amrex::Print() << "FBA LP solve failed for model " << cache.model_path
                   << " status=" << status << std::endl;
    return 0.0;
  }

  const double objective_value = objective->Value();
  return static_cast<Real>(objective_value);
#else
  amrex::Abort("BMX was built without OR-Tools support but use_fba_growth is enabled");
#endif
#endif
}
}
