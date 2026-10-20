#ifndef BMX_FBA_H_
#define BMX_FBA_H_

#include <AMReX_REAL.H>
#include <string>

namespace BMXFBA
{
  using amrex::Real;

  void initialize_from_sbml(const std::string& sbml_file);
  const std::string& sbml_model_path();

  Real compute_fba_growth_rate_hr(const Real* cell_par,
                                  const Real* p_vals,
                                  const Real* fluid_vals,
                                  Real volume_fraction,
                                  Real particle_count,
                                  int cell_type);
}

#endif
