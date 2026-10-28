#!/usr/bin/env bash
# Experimental two-stage ValkyrieRecomp -> R36S build.
# Stage 1 (x86_64 Linux): generate game C + free OpenBIOS C from your OWN discs.
# Stage 2 (x86_64 Linux): cross-compile them for a matching DarkOSRE AArch64 sysroot.
# Never uploads game discs, generated C or the resulting binary to GitHub.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
MODE="${1:---all}"
case "$MODE" in
  --generate|--build|--all) ;;
  *)
    echo "Usage: bash r36s/build-r36s.sh [--generate|--build|--all]" >&2
    exit 2
    ;;
esac

fatal() { printf '[R36S] ERROR: %s\n' "$*" >&2; exit 1; }
say() { printf '[R36S] %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1 || fatal "Missing host command '$1'"; }
JOBS="${R36S_BUILD_JOBS:-2}"
case "$JOBS" in ''|*[!0-9]*) fatal "R36S_BUILD_JOBS must be a positive integer";; esac
[ "$JOBS" -ge 1 ] || fatal "R36S_BUILD_JOBS must be >=1"
for tool in cmake ninja python3; do have "$tool"; done
[ -f psxrecomp/runtime/runtime.cmake ] ||
    fatal "Missing psxrecomp submodule. Run: git submodule update --init --recursive"

if [ "$MODE" = --generate ] || [ "$MODE" = --all ]; then
    # Generation is a HOST operation: never attempt it on a memory-limited R36S.
    [ "$(uname -m)" = x86_64 ] ||
        fatal "Generate on an x86_64 Linux/WSL host (not the R36S)"
    DISC="${VALKYRIE_DISC1_CUE:-$ROOT/disc/Disc1/Valkyrie Profile (USA) (Disc 1).cue}"
    [ -f "$DISC" ] || fatal "Disc 1 CUE not found: $DISC"
    [ -f game.toml ] || fatal "Missing game.toml"
    have gcc
    have g++
    say "Building PSXRecomp emitter tools (host x86_64)..."
    bash psxrecomp/tools/ci/build_emitters.sh --jobs "$JOBS"
    say "Generating recompiled MIT OpenBIOS backend..."
    (
        cd psxrecomp
        PSXRECOMP_BIOS_BUILD="../build-recompiler" \
            bash tools/regen_bios.sh --config bios/OpenBIOS.toml
    )
    [ -f psxrecomp/generated/OpenBIOS_dispatch.c ] ||
        fatal "OpenBIOS dispatcher was not generated"
    say "Generating game C from your local Disc 1..."
    python3 psxrecomp/psxrecomp_cli.py generate \
        --config game.toml --project-root "$ROOT" --disc "$DISC"
    [ -f generated/SLUS_011.56_dispatch.c ] ||
        fatal "Game C dispatcher not generated; examine generator output"
    say "Host generation succeeded. Generated C stays local and is gitignored."
fi

if [ "$MODE" = --build ] || [ "$MODE" = --all ]; then
    [ -f generated/SLUS_011.56_dispatch.c ] ||
        fatal "Game C missing. First run: bash r36s/build-r36s.sh --generate"
    [ -f psxrecomp/generated/OpenBIOS_dispatch.c ] ||
        fatal "OpenBIOS C missing. First run: bash r36s/build-r36s.sh --generate"
    [ -n "${R36S_SYSROOT:-}" ] ||
        fatal "Set R36S_SYSROOT=/absolute/path/to/a matching DarkOSRE development sysroot"
    [ -d "$R36S_SYSROOT" ] || fatal "Sysroot missing: $R36S_SYSROOT"
    have aarch64-linux-gnu-gcc
    have aarch64-linux-gnu-g++
    have pkg-config

    say "Configuring the real AArch64 game build (not a setup-only host)..."
    cmake --preset r36s-arm64-release
    say "Cross-compiling for Cortex-A35; this can use substantial HOST RAM/disk..."
    cmake --build --preset r36s-arm64-release --parallel "$JOBS"

    BIN="$ROOT/build-r36s-arm64-release/ValkyrieRecomp"
    [ -s "$BIN" ] || fatal "CMake finished but executable is missing: $BIN"
    have file
    have readelf
    say "Output architecture and ELF header:"
    file "$BIN"
    readelf -h "$BIN" | grep -E 'Class:|Machine:|Type:'
    file "$BIN" | grep -q 'aarch64\|ARM aarch64' ||
        fatal "Output is not confirmed as AArch64 ELF"

    DEST="$ROOT/build-r36s-package/ports/valkyrierecomp"
    mkdir -p "$DEST/disc/Disc1" "$DEST/disc/Disc2" "$DEST/saves" "$DEST/lib"
    cp -f "$BIN" "$DEST/ValkyrieRecomp"
    cp -f r36s/ValkyrieRecomp.sh game.toml game_options.toml "$DEST/"
    cp -f r36s/PortMaster/ValkyrieRecomp.sh "$ROOT/build-r36s-package/ports/ValkyrieRecomp.sh"
    chmod +x "$DEST/ValkyrieRecomp" "$DEST/ValkyrieRecomp.sh" \
        "$ROOT/build-r36s-package/ports/ValkyrieRecomp.sh"
    if [ -d "$ROOT/build-r36s-arm64-release/bios" ]; then
        cp -a "$ROOT/build-r36s-arm64-release/bios" "$DEST/"
    fi
    say "Staged: $ROOT/build-r36s-package/ports"
    say "Game discs are deliberately NOT copied. Add your legally owned BIN/CUE on SD."
    say "NOT hardware-validated: inspect ELF interpreter, GLIBC and linked libs on R36S."
fi
