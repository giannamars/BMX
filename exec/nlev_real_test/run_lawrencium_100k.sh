#!/bin/bash
#
# -------------------------------------------------
#  SLURM directives
# -------------------------------------------------
#SBATCH --job-name=bmx_nlevreal100k
#SBATCH --partition=es1
#SBATCH --qos=es_debug
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4
#SBATCH --gpus-per-task=1
#SBATCH --cpus-per-task=4
#SBATCH --time=4:00:00
#SBATCH --output=bmx_nlevreal100k_%j.out
#SBATCH --error=bmx_nlevreal100k_%j.err
#SBATCH -A pc_ldrd25mars
#SBATCH --chdir=/global/home/users/glmarschmann/BMXs/build/exec/nlev_real_test

# -------------------------------------------------
#  Modules / environment
# -------------------------------------------------
module purge
module load gcc/11.4.0 cuda/12.2.1 openmpi/4.1.6
export AMREX_CUDA_ARCH=70
export OMP_NUM_THREADS=1

# -------------------------------------------------
#  Run the 100k-step deck
# -------------------------------------------------
srun ../../bmx input_real_nlev
