#!/bin/bash

#SBATCH --job-name="jacobi_final"
#SBATCH --time=00:15:00
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=4
#SBATCH --cpus-per-task=8
#SBATCH --gres=gpu:4
#SBATCH --exclusive
#SBATCH --partition=boost_usr_prod
##SBATCH --qos=boost_qos_dbg
#SBATCH --account=ICT25_MHPC_0
#SBATCH --output=logs/jacobi_%j.out 
#SBATCH --error=logs/jacobi_%j.err

mkdir -p logs
module purge
module load singularity
module load openmpi/4.1.6--gcc--12.2.0-cuda-12.1

# These must match the internal structure of your .sif file
INT_HPCX="/opt/nvidia/hpc_sdk/Linux_x86_64/24.5/comm_libs/12.4/hpcx/hpcx-2.19/ompi"
INT_UCX="/opt/nvidia/hpc_sdk/Linux_x86_64/24.5/comm_libs/12.4/hpcx/hpcx-2.19/ucx"

# These variables are automatically injected inside the container
export SINGULARITYENV_OPAL_PREFIX="$INT_HPCX"
export SINGULARITYENV_LD_LIBRARY_PATH="$INT_HPCX/lib:$INT_UCX/lib:/usr/local/nvidia/lib64"
export SINGULARITYENV_PATH="$INT_HPCX/bin:$INT_UCX/bin:$PATH"

# Forces the use of UCX and the Mellanox Infiniband cards (mlx5_0)
export SINGULARITYENV_OMPI_MCA_pml=ucx
export SINGULARITYENV_OMPI_MCA_btl=^openib,tcp
export SINGULARITYENV_UCX_NET_DEVICES="mlx5_0:1"
export SINGULARITYENV_UCX_TLS="self,sm,rc"
export OMP_NUM_THREADS=${SLURM_CPUS_PER_TASK}

echo "------------------------------------------------------------"
echo "Job started on: $(date)"
echo "Nodes allocated: $SLURM_JOB_NODELIST"
echo "Internal MPI Root: $INT_HPCX"
echo "------------------------------------------------------------"

srun --mpi=pmi2 \
     singularity exec --nv --cleanenv \
     jacobi_new.sif /app/app.x jacobian.in

echo "------------------------------------------------------------"
echo "Job finished on: $(date)"
echo "------------------------------------------------------------"