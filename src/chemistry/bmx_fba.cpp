#include <bmx_fba.H>

#include <AMReX.H>

#ifdef BMX_USE_LIBSBML
#include <sbml/SBMLTypes.h>
#endif

#include <memory>

namespace BMXFBA
{
namespace
{
  std::string model_path;
}

void
initialize_from_sbml(const std::string& sbml_file)
{
  model_path = sbml_file;

  if (model_path.empty()) {
    amrex::Abort("chem_species.fba_sbml_file must be set when use_fba_growth is enabled");
  }

#ifdef BMX_USE_LIBSBML
  std::unique_ptr<libsbml::SBMLDocument> document(libsbml::readSBML(model_path));
  if (!document) {
    amrex::Abort("Failed to create an SBML document from " + model_path);
  }

  if (document->getNumErrors() > 0) {
    document->printErrors();
    amrex::Abort("SBML document contains errors: " + model_path);
  }

  amrex::Print() << "Loaded FBA SBML model: " << model_path << std::endl;
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
