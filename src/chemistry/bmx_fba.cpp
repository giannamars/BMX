#include <bmx_fba.H>

namespace BMXFBA
{
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
