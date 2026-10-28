#include <bmx_fba.H>

#include <AMReX.H>
#include <AMReX_Print.H>

#include <algorithm>
#include <cmath>
#include <fstream>
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
    std::unordered_map<std::string, std::string> lower_bound_ids;
    std::unordered_map<std::string, std::string> upper_bound_ids;
    std::unordered_map<std::string, double> parameter_values;
    std::unordered_map<std::string, double> objective_coefficients;
    bool objective_is_maximize = true;
    bool ready = false;
  };

  FbaCache cache;

#ifdef BMX_USE_LIBSBML
  std::string
  xml_attribute(const std::string& line, const std::string& attribute)
  {
    const std::string key = attribute + "=\"";
    const std::size_t begin = line.find(key);
    if (begin == std::string::npos) {
      return {};
    }

    const std::size_t value_begin = begin + key.size();
    const std::size_t value_end = line.find('"', value_begin);
    if (value_end == std::string::npos) {
      return {};
    }

    return line.substr(value_begin, value_end - value_begin);
  }

  void
  parse_sbml_metadata(const std::string& path)
  {
    cache.lower_bound_ids.clear();
    cache.upper_bound_ids.clear();
    cache.parameter_values.clear();
    cache.objective_coefficients.clear();
    cache.objective_is_maximize = true;

    std::ifstream input(path);
    if (!input) {
      amrex::Abort("Failed to open SBML file for metadata parsing: " + path);
    }

    std::string line;
    while (std::getline(input, line)) {
      if (line.find("<parameter") != std::string::npos) {
        const std::string parameter_id = xml_attribute(line, "id");
        const std::string parameter_value = xml_attribute(line, "value");
        if (!parameter_id.empty() && !parameter_value.empty()) {
          cache.parameter_values[parameter_id] = std::stod(parameter_value);
        }
      }

      if (line.find("<reaction") != std::string::npos) {
        const std::string reaction_id = xml_attribute(line, "id");
        if (!reaction_id.empty()) {
          const std::string lower_id = xml_attribute(line, "fbc:lowerFluxBound");
          const std::string upper_id = xml_attribute(line, "fbc:upperFluxBound");
          if (!lower_id.empty()) {
            cache.lower_bound_ids[reaction_id] = lower_id;
          }
          if (!upper_id.empty()) {
            cache.upper_bound_ids[reaction_id] = upper_id;
          }
        }
      }

      if (line.find("<fbc:objective") != std::string::npos) {
        const std::string type = xml_attribute(line, "fbc:type");
        cache.objective_is_maximize = (type != "minimize");
      }

      if (line.find("<fbc:fluxObjective") != std::string::npos) {
        const std::string reaction_id = xml_attribute(line, "fbc:reaction");
        const std::string coefficient = xml_attribute(line, "fbc:coefficient");
        if (!reaction_id.empty()) {
          const double value = coefficient.empty() ? 1.0 : std::stod(coefficient);
          cache.objective_coefficients[reaction_id] += value;
        }
      }
    }
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

    for (unsigned int i = 0; i < num_reactions; ++i) {
      const Reaction* reaction = model->getReaction(i);
      if (!reaction) {
        continue;
      }

      ReactionData data;
      data.id = reaction->getId();

      const double default_lower = reaction->getReversible() ? -1000.0 : 0.0;
      const double default_upper = 1000.0;
      const auto lower_id_iter = cache.lower_bound_ids.find(data.id);
      if (lower_id_iter != cache.lower_bound_ids.end()) {
        const auto value_iter = cache.parameter_values.find(lower_id_iter->second);
        data.lower_bound = (value_iter != cache.parameter_values.end()) ? value_iter->second : default_lower;
      } else {
        data.lower_bound = default_lower;
      }

      const auto upper_id_iter = cache.upper_bound_ids.find(data.id);
      if (upper_id_iter != cache.upper_bound_ids.end()) {
        const auto value_iter = cache.parameter_values.find(upper_id_iter->second);
        data.upper_bound = (value_iter != cache.parameter_values.end()) ? value_iter->second : default_upper;
      } else {
        data.upper_bound = default_upper;
      }

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

    for (auto& reaction : cache.reactions) {
      const auto coefficient_iter = cache.objective_coefficients.find(reaction.id);
      if (coefficient_iter != cache.objective_coefficients.end()) {
        reaction.objective_coefficient = coefficient_iter->second;
      }
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

    if (cache.objective_coefficients.empty()) {
      const std::string biomass_reaction_id = "R_treebiomasspseudoreaction";
      cache.objective_coefficients[biomass_reaction_id] = 1.0;
      for (auto& reaction : cache.reactions) {
        if (reaction.id == biomass_reaction_id) {
          reaction.objective_coefficient = 1.0;
        }
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
  parse_sbml_metadata(cache.model_path);

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
