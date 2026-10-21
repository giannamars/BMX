#include <bmx_fba.H>

#include <AMReX.H>
#include <AMReX_Print.H>

#ifdef BMX_USE_LIBSBML
#include <sbml/SBMLReader.h>
#include <sbml/SBMLDocument.h>
#include <sbml/Model.h>
#endif

#ifdef BMX_USE_ORTOOLS
#include "ortools/linear_solver/linear_solver.h"
#endif

namespace BMXFBA
{
namespace
{
  std::string model_path;

#ifdef BMX_USE_ORTOOLS
  Real
  run_ortools_smoke_test()
  {
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

    const Real objective_value = objective->Value();
    amrex::Print() << "OR-Tools LP smoke test objective = "
                   << objective_value << std::endl;
    return objective_value;
  }
#endif
}

void
initialize_from_sbml(const std::string& sbml_file)
{
  model_path = sbml_file;

  if (model_path.empty()) {
    amrex::Abort("chem_species.fba_sbml_file must be set when use_fba_growth is enabled");
  }

#ifdef BMX_USE_LIBSBML
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(model_path);
  if (!document) {
    amrex::Abort("Failed to create an SBML document from " + model_path);
  }

  if (document->getNumErrors() > 0) {
    document->printErrors();
    amrex::Abort("SBML document contains errors: " + model_path);
  }

  if (document->getModel() == nullptr) {
    delete document;
    amrex::Abort("SBML document has no model: " + model_path);
  }

#ifdef BMX_USE_ORTOOLS
  (void)run_ortools_smoke_test();
#endif

  amrex::Print() << "Loaded FBA SBML model: " << model_path << std::endl;
  delete document;
#else
  amrex::Abort("BMX was built without libSBML support but an SBML file was requested");
#endif
}

const std::string&
sbml_model_path()
{
  return model_path;
}

Real
compute_fba_growth_rate_hr(const Real* /*cell_par*/,
                           const Real* /*p_vals*/,
                           const Real* /*fluid_vals*/,
                           Real /*volume_fraction*/,
                           Real /*particle_count*/,
                           int /*cell_type*/)
{
  return 0.0;
}
}
