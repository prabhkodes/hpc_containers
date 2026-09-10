# # Use a slim Ubuntu base for efficiency
# FROM --platform=linux/amd64 ubuntu:22.04

# # Metadata
# LABEL Description="OpenMP FFT for Leonardo (built on macOS)"

# # Install build tools
# RUN apt-get update && apt-get install -y --no-install-recommends \
#     build-essential \
#     && rm -rf /var/lib/apt/lists/*

# WORKDIR /app

# # Copy your source code (make sure the file is named fft_omp.c)
# COPY fft.c .

# # Compile with OpenMP and aggressive optimization
# # -march=skylake-avx512 is great for Leonardo's Intel Ice Lake nodes
# RUN gcc -O3 -fopenmp fft.c -o fft.x -lm

# # Environment setup
# ENV OMP_NUM_THREADS=1
# ENTRYPOINT ["./fft.x"]


# Use a slim Ubuntu base for efficiency
FROM --platform=linux/amd64 ubuntu:22.04

# Metadata
LABEL Description="OpenMP FFT optimized for Leonardo (Intel Ice Lake)"

# Install build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 1. Ensure this matches the filename you saved!
# If you saved the previous code as fft_omp.c, use that here.
COPY fft_omp.c .

# 2. Optimized compilation for Ice Lake
# -march=icelake-server: specific to Leonardo's nodes
# -ffast-math: improves speed for complex number rotations
RUN gcc -O3 -fopenmp -march=icelake-server -ffast-math fft_omp.c -o fft.x -lm

# 3. HPC Environment setup for OpenMP
ENV OMP_NUM_THREADS=4
ENV OMP_PROC_BIND=true
ENV OMP_PLACES=cores

ENTRYPOINT ["./fft.x"]