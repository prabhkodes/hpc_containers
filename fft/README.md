# FFT — Containerised OpenMP Build

Cooley-Tukey FFT in C, packaged for portable deployment on HPC clusters via Docker and Singularity. Two source files: `fft.c` (serial) and `fft_omp.c` (OpenMP parallelised butterfly passes).

The container targets Leonardo's Intel Ice Lake nodes — compiled with `-march=icelake-server -ffast-math` so the image produces the right instruction set without needing the exact cluster environment locally.

## Build & Run (Docker)

```bash
docker build -t fft_omp .
docker run --rm fft_omp <N>   # N must be a power of 2, e.g. 1024
```

## Build & Run (Singularity — for clusters that don't allow Docker)

```bash
singularity build fft_omp.sif fft_openmp.def
singularity run fft_omp.sif <N>
```

## Build & Run (local, no container)

```bash
# Serial
gcc -O3 -march=native -o fft.x fft.c -lm
./fft.x 1024

# OpenMP
gcc -O3 -march=native -fopenmp -o fft_omp.x fft_omp.c -lm
OMP_NUM_THREADS=4 ./fft_omp.x 1024
```
