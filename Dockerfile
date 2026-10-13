# Build/dev image for the virtio-mmio device-model lab.
# Firecracker + guest-kernel build deps. Assumes the host exposes /dev/kvm.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential bc bison flex libelf-dev libssl-dev cpio \
        gcc-multilib libc6-dev-i386 ca-certificates curl git sudo \
        iproute2 iputils-ping && \
    rm -rf /var/lib/apt/lists/*

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
        sh -s -- -y --default-toolchain 1.97.0
ENV PATH="/root/.cargo/bin:${PATH}"

WORKDIR /lab
