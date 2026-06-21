# Jacobi GPU (MPI + OpenACC, containerised)

2D Jacobi heat diffusion solver offloaded to GPU via OpenACC, distributed across nodes with MPI. The container packages the full build environment so it runs portably on any GPU cluster without manual module setup.

## Container

The Dockerfile uses Ubuntu 22.04 + GCC 12 with OpenACC GPU offload (`gcc-12-offload-nvptx`) rather than the NVIDIA HPC SDK base image. MPI is built with UCX, PMIx, and InfiniBand verbs support baked in. Compiles for A100 (`-foffload=-march=sm_80`).

```bash
docker build -t jacobi_gpu .
docker run --gpus all jacobi_gpu
```

## SLURM Scripts

Four scripts cover different deployment and analysis scenarios on Leonardo Booster:

**`job.sh` — Native production run (8 nodes)**
Loads `hpcx-mpi` + `nvhpc`, compiles with `nvc++`, and launches with `srun`. The standard path for scaling runs.

```bash
sbatch job.sh
```

**`singularity.sh` — Containerised run (2 nodes)**
Runs the pre-built `.sif` image with `srun --mpi=pmi2 singularity exec --nv`. MPI and UCX paths inside the container are set via `SINGULARITYENV_*` to match the internal HPC-X layout, and InfiniBand is forced over `mlx5_0`.

```bash
sbatch singularity.sh
```

**`inifinband_force.sh` — Native run with explicit InfiniBand overrides (2 nodes)**
Same compile-and-run flow as `job.sh` but manually overrides `OPAL_PREFIX`, `LD_LIBRARY_PATH`, and UCX settings to force HPC-X over InfiniBand. Useful when the default module load picks up the wrong MPI stack and causes hangs on multi-node jobs.

```bash
sbatch inifinband_force.sh
```

**`nsys_profile.sh` — Nsight Systems profiling (1 node)**
Wraps execution with `nsys profile --trace=cuda,mpi,openacc,nvtx`, producing a `.nsys-rep` file per rank. Open in the Nsight Systems GUI to inspect the GPU/MPI/OpenACC timeline.

```bash
sbatch nsys_profile.sh
# Output: jacobi_profile_rank0.nsys-rep, jacobi_profile_rank1.nsys-rep, ...
```

## Input

`jacobian.in` sets the grid size, corner heat value, number of steps, and print interval.
