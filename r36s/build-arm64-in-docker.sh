#!/usr/bin/env bash
# Run only INSIDE the linux/arm64 PortMaster-derived build container.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
die() { echo "[R36S-Docker] ERROR: $*" >&2; exit 1; }

[ "$(uname -m)" = "aarch64" ] || die "Expected aarch64 container"
[ -f generated/SLUS_011.56_dispatch.c ] ||
    die "No game C: first run 'bash r36s/build-r36s.sh --generate' on an x86_64 host"
[ -f psxrecomp/generated/OpenBIOS_dispatch.c ] ||
    die "No OpenBIOS C: first generate on an x86_64 host"

JOBS="${R36S_BUILD_JOBS:-2}"
case "$JOBS" in ''|*[!0-9]*) die "R36S_BUILD_JOBS must be positive integer";; esac
[ "$JOBS" -ge 1 ] || die "R36S_BUILD_JOBS must be >=1"

echo "[R36S-Docker] Building real game runtime for AArch64 / Cortex-A35"
cmake -S . -B build-r36s-docker -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DVALKYRIE_R36S=ON \
    -DPSX_DEBUG_TOOLS=OFF \
    -DPSXRECOMP_REQUIRE_GAME_C=ON
cmake --build build-r36s-docker --target psx-runtime --parallel "$JOBS"

BIN="$ROOT/build-r36s-docker/ValkyrieRecomp"
[ -s "$BIN" ] || die "Expected binary not found: $BIN"
file "$BIN"
readelf -h "$BIN" | grep -E 'Class:|Machine:|Type:'
file "$BIN" | grep -qi 'ELF.*aarch64\|ELF.*ARM aarch64' ||
    die "Not a confirmed AArch64 ELF"

DEST="$ROOT/build-r36s-package/ports/valkyrierecomp"
mkdir -p "$DEST/disc/Disc1" "$DEST/disc/Disc2" "$DEST/saves" "$DEST/lib"
cp -f "$BIN" "$DEST/ValkyrieRecomp"
cp -f game.toml game_options.toml r36s/ValkyrieRecomp.sh "$DEST/"
cp -f r36s/PortMaster/ValkyrieRecomp.sh "$ROOT/build-r36s-package/ports/ValkyrieRecomp.sh"
chmod +x "$DEST/ValkyrieRecomp" "$DEST/ValkyrieRecomp.sh" \
    "$ROOT/build-r36s-package/ports/ValkyrieRecomp.sh"
if [ -d "$ROOT/build-r36s-docker/bios" ]; then
    cp -a "$ROOT/build-r36s-docker/bios" "$DEST/"
fi

echo "[R36S-Docker] ELF shared-library requirements (DO NOT bundle glibc):"
readelf -d "$BIN" | grep NEEDED || :
echo "[R36S-Docker] Staged at: $ROOT/build-r36s-package/ports"
echo "[R36S-Docker] Disc images NOT included: copy your own BIN/CUE after deploying."
echo "[R36S-Docker] Binary is NOT yet validated on DarkOSRE hardware."
