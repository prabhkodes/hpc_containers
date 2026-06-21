#!/bin/bash
#SBATCH --job-name="jacobi_native"
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=4
#SBATCH --cpus-per-task=8
#SBATCH --gres=gpu:4
#SBATCH --exclusive
#SBATCH --partition=boost_usr_prod
#SBATCH --account=ICT25_MHPC_0
#SBATCH --qos=boost_qos_dbg
#SBATCH --output=logs/jacobi_%j.out 
#SBATCH --error=logs/jacobi_%j.err

mkdir -p logs
module purge


module load gcc/12.2.0
module load nvhpc/24.5
module load openmpi/4.1.6--gcc--12.2.0-cuda-12.1


REAL_NV_ROOT="/leonardo/prod/spack/06/install/0.22/linux-rhel8-icelake/gcc-8.5.0/nvhpc-24.5-torlmnyzcexnrs6pq4cccabv7ehkv3xy/Linux_x86_64/24.5"
REAL_MPI_ROOT="$REAL_NV_ROOT/comm_libs/12.4/hpcx/hpcx-2.19/ompi"
REAL_UCX_ROOT="$REAL_NV_ROOT/comm_libs/12.4/hpcx/hpcx-2.19/ucx"

export OPAL_PREFIX="$REAL_MPI_ROOT"
export LD_LIBRARY_PATH="$REAL_MPI_ROOT/lib:$REAL_UCX_ROOT/lib:$LD_LIBRARY_PATH"
export PATH="$REAL_MPI_ROOT/bin:$PATH"


export OMPI_MCA_pml=ucx
export OMPI_MCA_btl=^openib,tcp
export UCX_NET_DEVICES="mlx5_0:1"
export UCX_TLS="self,sm,rc,cuda_copy,cuda_ipc"

export OMPI_MCA_coll_hcoll_enable=0
export HCOLL_ENABLE_MCAST=0


export OMPI_CXX=nvc++
export OMP_NUM_THREADS=${SLURM_CPUS_PER_TASK}
GCC_ROOT=$(dirname $(dirname $(which gcc)))

echo "[BUILD] Compiling..."
mpic++ -O3 -acc -gpu=cc80 \
    -std=c++20 --gcc-toolchain=$GCC_ROOT \
    -Iinclude -D_OPENACC \
    src/main.cpp -o app.x \
    -Minfo=acc 

if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed!"
    exit 1
fi

echo "[RUN] Launching with srun --mpi=pmix"

srun --mpi=pmix ./app.x "./jacobian.in"