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
#include <sstream>

using PinnedTile = typename BMXParticleContainer::ContainerLike<amrex::PinnedArenaAllocator>::ParticleTileType;

namespace
{
  struct DivisionLogState {
    bool initialized = false;
    std::ofstream stream;
  };

  DivisionLogState& division_log_state()
  {
    static DivisionLogState state;
    return state;
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

    for (amrex::MFIter mfi(xk, amrex::TilingIfNotGPU()); mfi.isValid(); ++mfi) {
      if (mfi.validbox().contains(iv)) {
        return xk.const_array(mfi)(i, j, k, 0);
      }
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
    if (write_header) {
      state.stream
        << "run_seed,rank,time,parent_id,daughter1_id,daughter2_id,"
        << "parent_volume_pre,daughter1_volume,daughter2_volume,"
        << "daughter1_x,daughter1_y,daughter1_z,"
        << "daughter2_x,daughter2_y,daughter2_z,"
        << "A_out_local,"
        << "daughter1_A_amount,daughter2_A_amount,"
        << "daughter1_P_amount,daughter2_P_amount,"
        << "parent_P_amount_pre,partition_mode,p_receiver,p_mass_diff,p_mass_tol\n";
    }
    state.initialized = true;
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
  const int l_run_seed = bmxchem->getDivisionEventRunSeed();

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

        if (l_log_divisions) {
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

            auto& log = division_log_state().stream;
            log << l_run_seed << ','
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
                << p_mass_tol << '\n';
          }
          division_log_state().stream.flush();
        }
      } // pti
  } // lev

  // Redistribute the particles so the new particles end up on the right processor
  Redistribute();
}
