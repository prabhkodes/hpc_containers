# HPC Containers

![C++](https://img.shields.io/badge/C++-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![C](https://img.shields.io/badge/C-A8B9CC?style=flat-square&logo=c&logoColor=black)
![CUDA](https://img.shields.io/badge/CUDA-76B900?style=flat-square&logo=nvidia&logoColor=white)
![OpenACC](https://img.shields.io/badge/OpenACC-FF6200?style=flat-square&logoColor=white)
![OpenMPI](https://img.shields.io/badge/OpenMPI-364d6e?style=flat-square&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=flat-square&logo=docker&logoColor=white)
![Singularity](https://img.shields.io/badge/Singularity-1C2D59?style=flat-square&logoColor=white)

HPC clusters typically restrict software installation and environment modules differ between systems. These projects package the build environment into containers so the same image runs consistently on any cluster that supports Docker or Singularity — no manual dependency setup required.

## Projects

### jacobi_gpu
2D Jacobi heat diffusion solver offloaded to GPU via OpenACC, with MPI for multi-node distribution.

The Dockerfile uses Ubuntu 22.04 + GCC 12 with OpenACC GPU offload (`gcc-12-offload-nvptx`) rather than the NVIDIA HPC SDK base image, keeping the image lighter and more portable. MPI communication uses UCX over InfiniBand with PMIx process management.

**Build (Docker):**
```bash
docker build -t jacobi_gpu .
docker run --gpus all jacobi_gpu
```

**SLURM scripts** — four deployment scenarios are covered:

| Script | Nodes | Description |
|---|---|---|
| `job.sh` | 8 | Native production run — hpcx-mpi + nvc++ + OpenACC |
| `singularity.sh` | 2 | Containerised run — Singularity with MPI/UCX injected via `SINGULARITYENV_*` |
| `inifinband_force.sh` | 2 | Native run with explicit HPC-X/UCX path overrides to force InfiniBand |
| `nsys_profile.sh` | 1 | Nsight Systems profiling — traces CUDA, MPI, OpenACC, and NVTX per rank |

`jacobian.in` sets the grid size, corner heat value, number of steps, and checkpoint interval.

### fft
Cooley-Tukey FFT in C — serial (`fft.c`) and OpenMP parallelised (`fft_omp.c`). Packaged for both Docker and Singularity. The image compiles for Intel Ice Lake (`-march=icelake-server -ffast-math`) to match Leonardo's CPU nodes without needing the cluster environment locally.

```bash
# Docker
docker build -t fft_omp .
docker run --rm fft_omp <N>       # N must be a power of 2

# Singularity (clusters that block Docker)
singularity build fft_omp.sif fft_openmp.def
singularity run fft_omp.sif <N>
```

## Dependencies

- Docker or Singularity
- NVIDIA Container Toolkit (jacobi_gpu — for `--gpus all` support)
