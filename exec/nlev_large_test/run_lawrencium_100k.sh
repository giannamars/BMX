#!/bin/bash
#
# -------------------------------------------------
#  SLURM directives
# -------------------------------------------------
#SBATCH --job-name=bmx_nlev100k
#SBATCH --partition=es1
#SBATCH --qos=es_debug
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4
#SBATCH --gpus-per-task=1
#SBATCH --cpus-per-task=4
#SBATCH --time=12:00:00
#SBATCH --output=bmx_nlev100k_%j.out
#SBATCH --error=bmx_nlev100k_%j.err
#SBATCH -A pc_ldrd25mars
#SBATCH --chdir=/global/home/users/glmarschmann/BoltzmannMFX/build/exec/nlev_large_test

# -------------------------------------------------
#  Modules / environment
# -------------------------------------------------
module purge
module load gcc/11.4.0 cuda/12.2.1 openmpi/4.1.6
export AMREX_CUDA_ARCH=7.0
export OMP_NUM_THREADS=1
export OMPI_MCA_pml=ucx
export OMPI_MCA_btl=^vader,tcp,openib,uct
export UCX_NET_DEVICES=mlx5_1:1,mlx5_3:1

# -------------------------------------------------
#  Run the 100k-step deck
# -------------------------------------------------
srun ../../bmx input_real_nlev
