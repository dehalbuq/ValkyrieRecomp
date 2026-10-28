#!/usr/bin/env bash
# Build the AArch64 game runtime using a PortMaster-derived arm64 container.
# Run from WSL2 Ubuntu/Linux with Docker; generate game C locally first.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
MODE="${1:---all}"
case "$MODE" in --all|--build-only) ;; *)
  echo "Usage: bash r36s/build-docker.sh [--all|--build-only]" >&2
  exit 2
  ;;
esac
command -v docker >/dev/null 2>&1 ||
  { echo "Docker is required (Docker Desktop/WSL2 or Docker Engine)" >&2; exit 1; }

if [ "$MODE" = "--all" ]; then
    bash r36s/build-r36s.sh --generate
fi
[ -f generated/SLUS_011.56_dispatch.c ] ||
  { echo "Game C missing. Run --all with your local Disc 1 BIN/CUE" >&2; exit 1; }
[ -f psxrecomp/generated/OpenBIOS_dispatch.c ] ||
  { echo "OpenBIOS generated C missing." >&2; exit 1; }

IMAGE="${R36S_DOCKER_IMAGE:-valkyrie-r36s-builder}"
echo "[R36S] Building AArch64 PortMaster-based Docker image..."
docker build --platform linux/arm64 -f r36s/docker/Dockerfile -t "$IMAGE" r36s/docker
echo "[R36S] Building AArch64 code in container..."
docker run --rm --platform linux/arm64 \
    -v "$ROOT:/workspace" -w /workspace \
    -e R36S_BUILD_JOBS="${R36S_BUILD_JOBS:-2}" \
    "$IMAGE" bash r36s/build-arm64-in-docker.sh
echo "[R36S] Stage: $ROOT/build-r36s-package/ports"
