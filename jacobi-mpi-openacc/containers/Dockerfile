FROM --platform=linux/amd64 ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-12 g++-12 gcc-12-offload-nvptx \
    openmpi-bin libopenmpi-dev \
    libucx-dev libpmix-dev libibverbs-dev \
    ca-certificates flex m4 libnuma-dev \
    && rm -rf /var/lib/apt/lists/*


RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 --slave /usr/bin/g++ g++ /usr/bin/g++-12


WORKDIR /app
COPY . .

RUN mpic++ -O3 -std=c++20 -fopenacc -foffload=-march=sm_80 \
    -fcf-protection=none \
    -Iinclude \
    src/main.cpp -o app.x