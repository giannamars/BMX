//
//     Copyright (c) 2013 Battelle Memorial Institute
//     Licensed under modified BSD License. A copy of this license can be found
//     in the LICENSE file in the top level directory of this distribution.
//
#include <bmx.H>
#include <bmx_chem.H>
#include <bmx_chem_K.H>
#include <bmx_des_K.H>
#include <bmx_chem_species_parms.H>

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

using PinnedTile = typename BMXParticleContainer::ContainerLike<amrex::PinnedArenaAllocator>::ParticleTileType;

namespace
{
  struct DivisionLogState {
    bool initialized = false;
    std::ofstream stream;
    int next_event_id = 1;
    int cumulative_divisions = 0;
    int cumulative_low_a_divisions = 0;
    int step_divisions = 0;
    int step_low_a_divisions = 0;
    amrex::Real low_a_threshold = 0.0;
  };

  struct ColonyLogState {
    bool initialized = false;
    std::ofstream stream;
    std::ofstream snapshot_stream;
  };

  DivisionLogState& division_log_state()
  {
    static DivisionLogState state;
    return state;
  }

  ColonyLogState& colony_log_state()
  {
    static ColonyLogState state;
    return state;
  }

  amrex::Real interpolate_quantile(const std::vector<amrex::Real>& values, amrex::Real q)
  {
    if (values.empty()) {
      return 0.0;
    }
    if (values.size() == 1) {
      return values.front();
    }

    const amrex::Real clamped_q = std::min<amrex::Real>(1.0, std::max<amrex::Real>(0.0, q));
    const amrex::Real pos = clamped_q * static_cast<amrex::Real>(values.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = std::min<std::size_t>(lo + 1, values.size() - 1);
    const amrex::Real frac = pos - static_cast<amrex::Real>(lo);
    return values[lo] + frac * (values[hi] - values[lo]);
  }

  amrex::Real sample_local_A_out(const amrex::MultiFab& xk, const amrex::Geometry& geom,
                                 const amrex::RealVect& pos)
  {
    const auto plo = geom.ProbLoArray();
    const auto dxi = geom.InvCellSizeArray();
    const int i = static_cast<int>(amrex::Math::floor((pos[0] - plo[0]) * dxi[0]));
    const int j = static_cast<int>(amrex::Math::floor((pos[1] - plo[1]) * dxi[1]));
    const int k = static_cast<int>(amrex::Math::floor((pos[2] - plo[2]) * dxi[2]));
    const amrex::IntVect iv(i, j, k);

    const amrex::Box cell_box(iv, iv);
    const auto hits = xk.boxArray().intersections(cell_box);
    if (!hits.empty()) {
      const int fab_index = hits.front().first;
      const auto& fab = *xk.fabPtr(fab_index);
      return fab.const_array()(i, j, k, 0);
    }

    return 0.0;
  }

  void init_division_log(int run_seed)
  {
    auto& state = division_log_state();
    if (state.initialized) {
      return;
    }

    const int rank = amrex::ParallelDescriptor::MyProc();
    std::ostringstream filename;
    filename << "division_events_seed" << run_seed << "_rank" << rank << ".csv";

    std::ifstream check(filename.str());
    check.seekg(0, std::ios::end);
    const bool write_header = !(check.good() && check.tellg() > 0);
    check.close();

    state.stream.open(filename.str(), std::ios::out | std::ios::app);
    state.stream << std::setprecision(16);
    state.low_a_threshold = BMXChemistry::instance()->getLowAThreshold();
    if (write_header) {
      state.stream
        << "division_event_id,run_seed,rank,time,parent_id,daughter1_id,daughter2_id,"
        << "parent_volume_pre,daughter1_volume,daughter2_volume,"
        << "daughter1_x,daughter1_y,daughter1_z,"
        << "daughter2_x,daughter2_y,daughter2_z,"
        << "A_out_local,"
        << "daughter1_A_amount,daughter2_A_amount,"
        << "daughter1_P_amount,daughter2_P_amount,"
        << "parent_P_amount_pre,partition_mode,p_receiver,p_mass_diff,p_mass_tol,"
        << "division_volume_threshold,division_volume_units,low_a_threshold,low_a_units,low_a_basis\n";
    }
    state.initialized = true;
  }

  void init_colony_log(int run_seed)
  {
    auto& state = colony_log_state();
    if (state.initialized) {
      return;
    }

    const int rank = amrex::ParallelDescriptor::MyProc();
    std::ostringstream filename;
    filename << "colony_metrics_seed" << run_seed << "_rank" << rank << ".csv";

    std::ifstream check(filename.str());
    check.seekg(0, std::ios::end);
    const bool write_header = !(check.good() && check.tellg() > 0);
    check.close();

    state.stream.open(filename.str(), std::ios::out | std::ios::app);
    state.stream << std::setprecision(16);
    if (write_header) {
      state.stream
        << "run_seed,rank,time,step,total_cells,cumulative_divisions,"
        << "divisions_this_step,low_a_divisions_this_step,low_a_divisions_cumulative,"
        << "extracellular_A_remaining,total_storage_P,storage_mean,storage_std,"
        << "storage_min,storage_p25,storage_median,storage_p75,storage_max,"
        << "division_volume_threshold,division_volume_units,low_a_threshold,low_a_units,low_a_basis,"
        << "snapshot_interval,snapshot_basis\n";
    }
    state.initialized = true;

    std::ostringstream snap_filename;
    snap_filename << "cell_snapshots_seed" << run_seed << "_rank" << rank << ".csv";
    std::ifstream snap_check(snap_filename.str());
    snap_check.seekg(0, std::ios::end);
    const bool snap_write_header = !(snap_check.good() && snap_check.tellg() > 0);
    snap_check.close();

    state.snapshot_stream.open(snap_filename.str(), std::ios::out | std::ios::app);
    state.snapshot_stream << std::setprecision(16);
    if (snap_write_header) {
      state.snapshot_stream
        << "run_seed,rank,time,step,cell_id,x,y,z,volume,distance_to_Vcrit,"
        << "A_out_local,A_in_amount,P_amount,low_a_flag,low_a_threshold,low_a_units,low_a_basis\n";
    }
  }
}

/**
 * @brief this function splits particles if some criterion is met
 * @param time current time at which split occurs
 * @param X_k fluid concentration fields used to sample local A_out for logging
 */
void
BMXParticleContainer::split_particles (Real time, const amrex::Vector<amrex::MultiFab*>& X_k)
{
  Real l_max_vol  = SPECIES::max_vol;
  Real l_max_len  = SPECIES::max_len;
  Real l_max_rad  = SPECIES::max_rad;
  Real l_brnch_prob = SPECIES::brnch_prob;
  Real l_split_prob = SPECIES::split_prob;
  Real l_overlap  = BMXChemistry::p_overlap;
  int l_num_reals = BMXChemistry::p_num_reals;
  int l_num_ints  = BMXChemistry::p_num_ints;
  BMXChemistry *bmxchem = BMXChemistry::instance();
  bool l_use_storage_model = bmxchem->getUseStorageModel();
  int l_storage_partition_mode = bmxchem->getStoragePartitionMode();
  const bool l_log_divisions = bmxchem->getDivisionEventLogging();
  const bool l_log_colony = bmxchem->getColonyMetricsLogging();
  const int l_run_seed = bmxchem->getDivisionEventRunSeed();

  division_log_state().low_a_threshold = bmxchem->getLowAThreshold();

  if (l_log_divisions) {
    init_division_log(l_run_seed);
  }

  for (int lev = 0; lev <= finest_level; lev++) {

      MultiFab* xk_ptr = (X_k.size() > static_cast<std::size_t>(lev)) ? X_k[lev] : nullptr;

      for (BMXParIter pti(*this, lev); pti.isValid(); ++pti)
      {
        auto& particles = pti.GetArrayOfStructs();
        BMXParticleContainer::ParticleType* pstruct = particles().dataPtr();

        const int grid = pti.index();
        const int tile = pti.LocalTileIndex();
        auto& particle_tile = this->GetParticles(lev)[std::make_pair(grid,tile)];

        const int np = particles.size();

        PinnedTile pre_tile;
        if (l_log_divisions) {
          pre_tile.define(NumRuntimeRealComps(), NumRuntimeIntComps());
          pre_tile.resize(np);
          amrex::copyParticles(pre_tile, particle_tile, 0, 0, np);
        }

        // whether each particle will be split
        Gpu::DeviceVector<unsigned int> do_split(np+1, 0);
        auto do_split_p = do_split.data();
        amrex::ParallelForRNG( np, [=] AMREX_GPU_DEVICE (int i,
              amrex::RandomEngine const& engine) noexcept
        {
            int n_new;
            BMXParticleContainer::ParticleType& p = pstruct[i];
            if (checkSplit(&p.rdata(0), &p.idata(0), l_max_vol,
                  l_max_len, l_max_rad, l_brnch_prob, l_split_prob,
                  &n_new, engine))
            {
                do_split_p[i] = n_new;
            }
        });
        Gpu::Device::synchronize();

        // Prefix sum to count total number of new particles to create
        Gpu::DeviceVector<unsigned int> offsets(np+1);
        Gpu::exclusive_scan(do_split.begin(), do_split.end(), offsets.begin());
        unsigned int num_split;
#ifdef AMREX_USE_GPU
        Gpu::dtoh_memcpy(&num_split,offsets.dataPtr()+np,sizeof(unsigned int));
#else
        std::memcpy(&num_split,offsets.dataPtr()+np,sizeof(unsigned int));
#endif

        Vector<unsigned int> do_split_h(np);
        Vector<unsigned int> offsets_h(np+1);
#ifdef AMREX_USE_GPU
        Gpu::dtoh_memcpy(do_split_h.dataPtr(), do_split.dataPtr(), np*sizeof(unsigned int));
        Gpu::dtoh_memcpy(offsets_h.dataPtr(), offsets.dataPtr(), (np+1)*sizeof(unsigned int));
#else
        std::memcpy(do_split_h.dataPtr(), do_split.dataPtr(), np*sizeof(unsigned int));
        std::memcpy(offsets_h.dataPtr(), offsets.dataPtr(), (np+1)*sizeof(unsigned int));
#endif

        // make room for new particles - invalidates iterators, so get the ptr again
        particle_tile.resize(np+num_split);
        particle_tile.setNumNeighbors(0);
        pstruct = particles().dataPtr();

        // Update NextID to include particles created in this function
        Long next_pid;
#ifdef AMREX_USE_OMP
#pragma omp critical (add_plasma_nextid)
#endif
        {
            next_pid = ParticleType::NextID();
            ParticleType::NextID(next_pid+num_split);
        }
        Real split_len = SPECIES::split_len;

        // Fill new particle data. If particle pid is split, the new particle
        // is at index np + poffsets[pid];
        auto poffsets = offsets.data();
        int my_proc = amrex::ParallelDescriptor::MyProc();
        amrex::ParallelForRNG( np, [=] AMREX_GPU_DEVICE (int pid, amrex::RandomEngine const& engine) noexcept
        {
            BMXParticleContainer::ParticleType& p_orig = pstruct[pid];

            // Check to see if particle satisfies some criteria for splitting
            // into two new particles
            // Real* p_par =  &p_orig.rdata(0);
            if (do_split_p[pid] == 1)
            {
                // std::printf("MAKING NEW PARTICLE FROM VOL = %f \n", p_par[realIdx::vol]);
                ParticleType& p = pstruct[np+poffsets[pid]];
                p.id()  = next_pid + poffsets[pid];
                p.cpu() = my_proc;

                Real *pos_orig = &p_orig.pos(0);
                Real *pos_new  = &p.pos(0);

                Real *par_orig = &p_orig.rdata(0);
                Real *par_new  = &p.rdata(0);

                int *ipar_orig = &p_orig.idata(0);
                int *ipar_new  = &p.idata(0);

                // Set parameters on new particle base on values from
                // original particle
                if (ipar_orig[intIdx::cell_type] == cellType::YEAST) {
                  setNewCell(pos_orig, pos_new, par_orig,
                             par_new, ipar_orig, ipar_new,
                             l_overlap, l_num_reals, l_num_ints,
                             l_use_storage_model,
                             l_storage_partition_mode, engine);
                } else {
                  setNewSegment(pos_orig, pos_new, par_orig,
                                par_new, ipar_orig, ipar_new,
                                l_num_reals, l_num_ints, split_len,
                                p.id(), p.cpu(), engine);
                }
                ipar_new[intIdx::id] = p.id();
                ipar_new[intIdx::cpu] = p.cpu();
            } else if (do_split_p[pid] == 2) {

                ParticleType& p1 = pstruct[np+poffsets[pid]];
                p1.id()  = next_pid + poffsets[pid];
                p1.cpu() = my_proc;

                ParticleType& p2 = pstruct[np+poffsets[pid]+1];
                p2.id()  = next_pid + poffsets[pid]+1;
                p2.cpu() = my_proc;

                Real *pos_orig = &p_orig.pos(0);
                Real *pos_new1  = &p1.pos(0);
                Real *pos_new2  = &p2.pos(0);

                Real *par_orig = &p_orig.rdata(0);
                Real *par_new1  = &p1.rdata(0);
                Real *par_new2  = &p2.rdata(0);

                int *ipar_orig = &p_orig.idata(0);
                int *ipar_new1  = &p1.idata(0);
                int *ipar_new2  = &p2.idata(0);

                setNewSegments(pos_orig, pos_new1, pos_new2,
                               par_orig, par_new1, par_new2,
                               ipar_orig, ipar_new1, ipar_new2,
                               l_num_reals, l_num_ints, split_len,
                               p1.id(), p1.cpu(), p2.id(), p2.cpu(), engine);

            } // if test
        }); // pid

        Gpu::Device::synchronize();

        PinnedTile post_tile;
        post_tile.define(NumRuntimeRealComps(), NumRuntimeIntComps());
        post_tile.resize(np + num_split);
        amrex::copyParticles(post_tile, particle_tile, 0, 0, np + num_split);

        const auto& pre_particles = pre_tile.GetArrayOfStructs();
        const auto& post_particles = post_tile.GetArrayOfStructs();
        const auto& geom = Geom(lev);
        const MultiFab* xk_level = xk_ptr;

        for (int pid = 0; pid < np; ++pid) {
          if (do_split_h[pid] == 0) {
            continue;
          }

          const auto& pre_parent = pre_particles[pid];
          const Real parent_volume_pre = pre_parent.rdata(realIdx::vol);
          const Real parent_p_pre = pre_parent.rdata(realIdx::first_data + 4) * parent_volume_pre;
          const Real a_out_local = (xk_level != nullptr)
            ? sample_local_A_out(*xk_level, geom, pre_parent.pos())
            : 0.0;

          int daughter1_index = pid;
          int daughter2_index = np + offsets_h[pid];
          if (do_split_h[pid] == 2) {
            daughter1_index = np + offsets_h[pid];
            daughter2_index = np + offsets_h[pid] + 1;
          }
          const auto& daughter1 = post_particles[daughter1_index];
          const auto& daughter2 = post_particles[daughter2_index];

          const Real daughter1_vol = daughter1.rdata(realIdx::vol);
          const Real daughter2_vol = daughter2.rdata(realIdx::vol);
          const Real daughter1_A_amount = daughter1.rdata(realIdx::first_data + 0) * daughter1_vol;
          const Real daughter2_A_amount = daughter2.rdata(realIdx::first_data + 0) * daughter2_vol;
          const Real daughter1_P_amount = daughter1.rdata(realIdx::first_data + 4) * daughter1_vol;
          const Real daughter2_P_amount = daughter2.rdata(realIdx::first_data + 4) * daughter2_vol;
          const Real p_mass_sum = daughter1_P_amount + daughter2_P_amount;
          const Real p_mass_diff = parent_p_pre - p_mass_sum;
          const Real parent_p_abs = amrex::Math::abs(parent_p_pre);
          const Real p_mass_tol = std::max(Real(1.0e-14), Real(1.0e-10) * std::max(Real(1.0), parent_p_abs));
          const Real division_vol_threshold = SPECIES::max_vol;
          const Real low_a_threshold = bmxchem->getLowAThreshold();

          int partition_mode = l_storage_partition_mode;
          int p_receiver = 0;
          if (l_use_storage_model && do_split_h[pid] == 1 &&
              pre_parent.idata(intIdx::cell_type) == cellType::YEAST) {
            if (daughter1_P_amount > daughter2_P_amount) {
              p_receiver = 1;
            } else if (daughter2_P_amount > daughter1_P_amount) {
              p_receiver = 2;
            }
          } else {
            partition_mode = -1;
            p_receiver = 0;
          }

          if (std::abs(p_mass_diff) > p_mass_tol) {
            std::ostringstream msg;
            msg << "P mass not conserved during division: parent=" << parent_p_pre
                << " daughters=" << p_mass_sum << " diff=" << p_mass_diff
                << " tol=" << p_mass_tol << " pid=" << pre_parent.id()
                << " time=" << time;
            amrex::Abort(msg.str().c_str());
          }

          auto& division_state = division_log_state();

          if (l_log_divisions) {
            const int division_event_id = division_state.next_event_id++;
            auto& log = division_state.stream;
            log << division_event_id << ','
                << l_run_seed << ','
                << amrex::ParallelDescriptor::MyProc() << ','
                << time << ','
                << pre_parent.id() << ','
                << daughter1.id() << ','
                << daughter2.id() << ','
                << parent_volume_pre << ','
                << daughter1_vol << ','
                << daughter2_vol << ','
                << daughter1.pos(0) << ',' << daughter1.pos(1) << ',' << daughter1.pos(2) << ','
                << daughter2.pos(0) << ',' << daughter2.pos(1) << ',' << daughter2.pos(2) << ','
                << a_out_local << ','
                << daughter1_A_amount << ',' << daughter2_A_amount << ','
                << daughter1_P_amount << ',' << daughter2_P_amount << ','
                << parent_p_pre << ','
                << partition_mode << ','
                << p_receiver << ','
                << p_mass_diff << ','
                << p_mass_tol << ','
                << division_vol_threshold << ','
                << "cm^3" << ','
                << low_a_threshold << ','
                << "concentration" << ','
                << "pre_exchange_A_out_local" << '\n';
          }

          if (l_log_divisions || l_log_colony) {
            division_state.cumulative_divisions += 1;
            division_state.step_divisions += 1;
            if (a_out_local <= division_state.low_a_threshold) {
              division_state.cumulative_low_a_divisions += 1;
              division_state.step_low_a_divisions += 1;
            }
          }
        }
        if (l_log_divisions) {
          division_log_state().stream.flush();
        }
      } // pti
  } // lev

  // Redistribute the particles so the new particles end up on the right processor
  Redistribute();
}

void
BMXParticleContainer::LogColonyMetrics (Real time, int nstep, Real extracellular_A_remaining,
                                        const amrex::Vector<const amrex::MultiFab*>& X_k)
{
  auto& division_state = division_log_state();
  const bool log_colony = BMXChemistry::instance()->getColonyMetricsLogging();
  if (!log_colony) {
    return;
  }

  init_colony_log(BMXChemistry::instance()->getDivisionEventRunSeed());
  const int snapshot_interval = BMXChemistry::instance()->getCellSnapshotInterval();
  const bool do_snapshot = (snapshot_interval > 0 && (nstep % snapshot_interval) == 0);
  const Real division_vol_threshold = SPECIES::max_vol;
  const Real low_a_threshold = division_state.low_a_threshold;

  std::vector<Real> storage_amounts;
  int total_cells = 0;

  for (int lev = 0; lev <= finest_level; ++lev) {
    int n_at_lev = this->NumberOfParticlesAtLevel(lev);
    if (n_at_lev == 0) {
      continue;
    }

    for (BMXParIter pti(*this, lev); pti.isValid(); ++pti) {
      auto& particle_tile = this->GetParticles(lev)[std::make_pair(pti.index(), pti.LocalTileIndex())];
      PinnedTile host_tile;
      host_tile.define(NumRuntimeRealComps(), NumRuntimeIntComps());
      host_tile.resize(particle_tile.numParticles());
      amrex::copyParticles(host_tile, particle_tile, 0, 0, particle_tile.numParticles());

      const auto& host_particles = host_tile.GetArrayOfStructs();
      for (int i = 0; i < host_particles.numParticles(); ++i) {
        const auto& particle = host_particles[i];
        if (particle.idata(intIdx::cell_type) == cellType::FUNGI) {
          continue;
        }

        const Real stored_amount = particle.rdata(realIdx::first_data + 4) * particle.rdata(realIdx::vol);
        storage_amounts.push_back(stored_amount);
        total_cells += 1;
      }
    }
  }

  std::sort(storage_amounts.begin(), storage_amounts.end());

  Real storage_sum = 0.0;
  Real storage_sum_sq = 0.0;
  for (const Real amount : storage_amounts) {
    storage_sum += amount;
    storage_sum_sq += amount * amount;
  }

  const int n = static_cast<int>(storage_amounts.size());
  const Real storage_mean = (n > 0) ? storage_sum / static_cast<Real>(n) : 0.0;
  const Real storage_var = (n > 1)
    ? std::max(Real(0.0), storage_sum_sq / static_cast<Real>(n) - storage_mean * storage_mean)
    : 0.0;
  const Real storage_std = std::sqrt(storage_var);
  const Real storage_min = (n > 0) ? storage_amounts.front() : 0.0;
  const Real storage_max = (n > 0) ? storage_amounts.back() : 0.0;
  const Real storage_p25 = interpolate_quantile(storage_amounts, 0.25);
  const Real storage_median = interpolate_quantile(storage_amounts, 0.50);
  const Real storage_p75 = interpolate_quantile(storage_amounts, 0.75);

  auto& state = colony_log_state();
  state.stream << BMXChemistry::instance()->getDivisionEventRunSeed() << ','
               << amrex::ParallelDescriptor::MyProc() << ','
               << time << ','
               << nstep << ','
               << total_cells << ','
               << division_state.cumulative_divisions << ','
               << division_state.step_divisions << ','
               << division_state.step_low_a_divisions << ','
               << division_state.cumulative_low_a_divisions << ','
               << extracellular_A_remaining << ','
               << storage_sum << ','
               << storage_mean << ','
               << storage_std << ','
               << storage_min << ','
               << storage_p25 << ','
               << storage_median << ','
               << storage_p75 << ','
               << storage_max << ','
               << division_vol_threshold << ','
               << "cm^3" << ','
               << low_a_threshold << ','
               << "concentration" << ','
               << "pre_exchange_A_out_local" << ','
               << snapshot_interval << ','
               << "per-cell snapshots" << '\n';
  state.stream.flush();

  if (do_snapshot) {
    for (int lev = 0; lev <= finest_level; ++lev) {
      int n_at_lev = this->NumberOfParticlesAtLevel(lev);
      if (n_at_lev == 0) {
        continue;
      }

      const MultiFab* xk_level = (X_k.size() > static_cast<std::size_t>(lev)) ? X_k[lev] : nullptr;
      const auto& geom = Geom(lev);

      for (BMXParIter pti(*this, lev); pti.isValid(); ++pti) {
        auto& particle_tile = this->GetParticles(lev)[std::make_pair(pti.index(), pti.LocalTileIndex())];
        PinnedTile host_tile;
        host_tile.define(NumRuntimeRealComps(), NumRuntimeIntComps());
        host_tile.resize(particle_tile.numParticles());
        amrex::copyParticles(host_tile, particle_tile, 0, 0, particle_tile.numParticles());

        const auto& host_particles = host_tile.GetArrayOfStructs();
        for (int i = 0; i < host_particles.numParticles(); ++i) {
          const auto& particle = host_particles[i];
          if (particle.idata(intIdx::cell_type) == cellType::FUNGI) {
            continue;
          }

          const Real volume = particle.rdata(realIdx::vol);
          const Real a_in_amount = particle.rdata(realIdx::first_data + 0) * volume;
          const Real p_amount = particle.rdata(realIdx::first_data + 4) * volume;
          const Real a_out_local = (xk_level != nullptr)
            ? sample_local_A_out(*xk_level, geom, particle.pos())
            : 0.0;

          state.snapshot_stream << BMXChemistry::instance()->getDivisionEventRunSeed() << ','
                                << amrex::ParallelDescriptor::MyProc() << ','
                                << time << ','
                                << nstep << ','
                                << particle.id() << ','
                                << particle.pos(0) << ','
                                << particle.pos(1) << ','
                                << particle.pos(2) << ','
                                << volume << ','
                                << (division_vol_threshold - volume) << ','
                                << a_out_local << ','
                                << a_in_amount << ','
                                << p_amount << ','
                                << (a_out_local <= low_a_threshold ? 1 : 0) << ','
                                << low_a_threshold << ','
                                << "concentration" << ','
                                << "pre_exchange_A_out_local" << '\n';
        }
      }
    }
    state.snapshot_stream.flush();
  }

  division_state.step_divisions = 0;
  division_state.step_low_a_divisions = 0;
}
