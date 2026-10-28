# Native AArch64 build image for an experimental R36S PortMaster port.
# Host must first generate copyrighted game C from their OWN local disc.
# Reuse the public PortMaster Ubuntu 20.04/AArch64 builder to target an older
# glibc baseline. It does not make the resulting binary firmware-independent.
FROM ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates file git ninja-build \
    python3 python3-pip pkg-config \
    libsdl2-dev zlib1g-dev libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu focal's apt CMake (3.16) is too old for ValkyrieRecomp (>= 3.20).
# The arm64 PyPI wheel installs a newer CMake without upgrading glibc.
RUN python3 -m pip install --no-cache-dir 'cmake==3.29.6' \
    && cmake --version
WORKDIR /workspace
