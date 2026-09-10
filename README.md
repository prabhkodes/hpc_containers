# hpc_containers

![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![C](https://img.shields.io/badge/C-A8B9CC?style=flat-square&logo=c&logoColor=black)
![MPI](https://img.shields.io/badge/MPI-364d6e?style=flat-square&logoColor=white)
![OpenACC](https://img.shields.io/badge/OpenACC-FF6200?style=flat-square&logoColor=white)
![OpenMP](https://img.shields.io/badge/OpenMP-006DB8?style=flat-square&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=flat-square&logo=docker&logoColor=white)
![Singularity](https://img.shields.io/badge/Singularity-1C2D59?style=flat-square&logoColor=white)
![UCX](https://img.shields.io/badge/UCX-5C4EE5?style=flat-square&logoColor=white)
![Nsight](https://img.shields.io/badge/Nsight-76B900?style=flat-square&logo=nvidia&logoColor=white)
![SLURM](https://img.shields.io/badge/SLURM-46a2f1?style=flat-square&logoColor=white)

A GPU-offloaded MPI solver has to run on machines you don't administer, where the module stack differs,
Docker is usually banned, and you have no root. **This is the same solver shipped four different ways**,
so you can see what each deployment route costs and where each one breaks.

| Route | Toolchain | Use it when |
|---|---|---|
| [`slurm/native.sh`](jacobi-mpi-openacc/slurm/native.sh) | NVHPC `nvc++ -acc -gpu=cc80` + hpcx-mpi modules | The cluster has the modules you need. The normal path |
| [`slurm/native-infiniband.sh`](jacobi-mpi-openacc/slurm/native-infiniband.sh) | Same, with `OPAL_PREFIX` / UCX forced by hand | The module-provided MPI picks the wrong transport and multi-node jobs hang |
| [`slurm/singularity.sh`](jacobi-mpi-openacc/slurm/singularity.sh) | Container image, MPI paths injected via `SINGULARITYENV_*` | The cluster forbids Docker, or you need a frozen environment |
| [`containers/Dockerfile`](jacobi-mpi-openacc/containers/Dockerfile) | GCC 12 `-fopenacc -foffload=-march=sm_80` | Local development on a laptop with no GPU and no cluster |

**What this repo demonstrates**

- **The same OpenACC source builds under two independent compilers** — GCC's `nvptx` offload and
  NVIDIA's `nvc++`. That's the actual portability claim, not a hypothetical one.
- **Cross-architecture image builds** — the container is built on macOS/arm64 and targets
  `linux/amd64` with `sm_80` device code.
- **MPI over InfiniBand from inside a container**, which is the part that actually breaks.
- **Profiling survives containerisation** — Nsight Systems traces CUDA, MPI, OpenACC and NVTX per rank.

**Stack:** C++20 · C · MPI (OpenMPI / HPC-X) · OpenACC · OpenMP · UCX · PMIx · Docker · Singularity ·
Nsight Systems · SLURM

**Where it ran:** Leonardo Booster at CINECA — 4× A100 64 GB per node, Mellanox HDR InfiniBand.

> **Related:** [`jacobi-poisson-solver`](https://github.com/prabhkodes/jacobi-poisson-solver) has the
> same Jacobi solver, but asks a different question — how four *parallel models* compare, and how they
> scale to 1120 cores and 40 GPUs. This repo is about **deployment and portability**, not performance.

## The hard part: MPI over InfiniBand inside a container

A containerised MPI program has two MPI stacks in play — the host launcher and the one baked into the
image. They have to agree, and if they don't the job either falls back to TCP (slow) or hangs.

The fix is to make the container use its *own* HPC-X and UCX, and to force the transport explicitly:

| Variable | Value | Why |
|---|---|---|
| `SINGULARITYENV_OPAL_PREFIX` | container's HPC-X path | Tells OpenMPI where its own installation lives |
| `SINGULARITYENV_LD_LIBRARY_PATH` | HPC-X + UCX + `/usr/local/nvidia/lib64` | Resolves MPI, UCX and the injected driver libraries |
| `OMPI_MCA_pml` | `ucx` | Use UCX for point-to-point, not the legacy BTL path |
| `OMPI_MCA_btl` | `^openib,tcp` | Explicitly *exclude* the fallbacks so a misconfiguration fails loudly instead of running slowly |
| `UCX_NET_DEVICES` | `mlx5_0:1` | Pin to the actual InfiniBand HCA rather than letting UCX guess |
| `UCX_TLS` | `self,sm,rc` (+ `cuda_copy,cuda_ipc` natively) | Shared memory on-node, reliable-connection IB off-node, CUDA transports for device buffers |
| `OMPI_MCA_coll_hcoll_enable` | `0` | HCOLL collectives were a source of hangs here; disabled |

Launch with `srun --mpi=pmi2` and `singularity exec --nv` — PMI2 for process management, `--nv` to
inject the host NVIDIA driver stack into the container.

→ **`native-infiniband.sh` applies exactly the same overrides without a container**, which is what
makes the pair useful: it isolates whether a problem is the container or the MPI configuration.

## Two OpenACC toolchains, one source

| | Container build | Native build |
|---|---|---|
| Compiler | GCC 12 | NVIDIA `nvc++` 24.5 |
| Offload flags | `-fopenacc -foffload=-march=sm_80` | `-acc -gpu=cc80` |
| Provenance | `apt install gcc-12-offload-nvptx` | NVIDIA HPC SDK module |
| Image weight | Ubuntu base + compiler, no vendor SDK | Requires the full SDK on the host |
| Extra flag needed | `-fcf-protection=none` — control-flow protection isn't supported on the nvptx target | `--gcc-toolchain=$GCC_ROOT` so `nvc++` finds a modern libstdc++ |

→ **Getting one scientific kernel through two unrelated OpenACC implementations is the portability
result.** Compiling with the vendor toolchain alone proves nothing about portability.

## Projects

### [`jacobi-mpi-openacc/`](jacobi-mpi-openacc/)

2-D Jacobi heat diffusion — GPU-offloaded with OpenACC, distributed with MPI, one GPU per rank.

<p align="center">
  <img src="jacobi-mpi-openacc/results/jacobi_diffusion.gif" width="520" alt="Jacobi iterations relaxing a hot corner">
</p>

<p align="center"><sub>The solver being shipped. Stitched from two MPI ranks — useful here as a smoke
test that a containerised run produces the same field as a native one.</sub></p>

```
src/ include/          solver
input/jacobian.in      grid size, corner value, steps, print interval
results/               reference animation
containers/Dockerfile  GCC offload build, cross-built to linux/amd64
containers/jacobi.def  Singularity recipe (converts the Docker image)
slurm/                 the four deployment routes above
```

```bash
# local container build
docker build -t jacobi_gpu -f jacobi-mpi-openacc/containers/Dockerfile jacobi-mpi-openacc
docker run --gpus all jacobi_gpu

# convert for a cluster that bans Docker
singularity build jacobi.sif jacobi-mpi-openacc/containers/jacobi.def

# on the cluster
sbatch jacobi-mpi-openacc/slurm/native.sh
SIF=/path/to/jacobi.sif sbatch jacobi-mpi-openacc/slurm/singularity.sh
```

### [`fft-openmp/`](fft-openmp/)

Cooley–Tukey FFT in C — serial and OpenMP — packaged for both Docker and Singularity. The CPU-side
counterpart: no GPU, no MPI, so it isolates the *cross-compilation* problem on its own.

- Built with `-march=icelake-server -ffast-math` to target Leonardo's CPU nodes
- Produces the right instruction set without needing the cluster environment locally
- `OMP_PROC_BIND=true` and `OMP_PLACES=cores` baked into the image

```bash
docker build -t fft_omp -f fft-openmp/containers/Dockerfile fft-openmp
docker run --rm fft_omp 1024                    # N must be a power of two

singularity build fft.sif fft-openmp/containers/fft_openmp.def
singularity run fft.sif 1024

# no container
gcc -O3 -march=native -fopenmp -o fft.x fft-openmp/src/fft_omp.c -lm
OMP_NUM_THREADS=4 ./fft.x 1024
```

## Profiling inside the toolchain

[`slurm/nsys-profile.sh`](jacobi-mpi-openacc/slurm/nsys-profile.sh) wraps the run in

```bash
nsys profile --trace=cuda,mpi,openacc,nvtx --stats=true \
  --output=jacobi_profile_rank%q{SLURM_PROCID}
```

producing one `.nsys-rep` per rank. Tracing `openacc` and `mpi` together is what lets you see whether a
stall is the kernel or the halo exchange.

## Caveats

| Caveat | Detail |
|---|---|
| **No scaling numbers here** | This repo is about deployment, not performance. Scaling for the same solver is in [`jacobi-poisson-solver`](https://github.com/prabhkodes/jacobi-poisson-solver) |
| **Paths are Leonardo-specific** | `native-infiniband.sh` hardcodes a Spack install prefix, and the Singularity script hardcodes the HPC-X layout inside the image. Both need editing for another site |
| **`.sif` is not committed** | Container images are too large for git. Build it from the Dockerfile via [`containers/jacobi.def`](jacobi-mpi-openacc/containers/jacobi.def) |
| **Host/container MPI must be ABI-compatible** | The usual containerised-MPI constraint. Both sides here are OpenMPI-derived |
| **GCC offload is the lighter path, not the faster one** | It keeps the image small and dependency-free; NVHPC generally generates better device code |

## Where this came from

| | |
|---|---|
| Course | *P2.3 — Cloud and Containers* and *P1.7 GPU Programming*, MHPC, ICTP / SISSA Trieste, 2025–26 |
| Solver | The Jacobi source is shared with [`jacobi-poisson-solver`](https://github.com/prabhkodes/jacobi-poisson-solver) |
| Cluster | Leonardo Booster, CINECA |
