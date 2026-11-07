#!/usr/bin/env bash
# Apply narrow build-compatibility patches to the pinned PSXRecomp checkout.
# Does not change the public PSXRecomp submodule pin or tracked game data.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
FRAMEWORK="$ROOT/psxrecomp"
PATCH="$ROOT/r36s/patches/psxrecomp-overlay-loader-label.patch"

if [ ! -f "$FRAMEWORK/runtime/src/overlay_loader.c" ]; then
    echo "[R36S] Missing PSXRecomp sources; initialize submodules first." >&2
    exit 1
fi

if git -C "$FRAMEWORK" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
    echo "[R36S] C11 compatibility patch already applied."
elif git -C "$FRAMEWORK" apply --check "$PATCH"; then
    git -C "$FRAMEWORK" apply "$PATCH"
    echo "[R36S] Applied overlay_loader.c C11 label compatibility patch."
else
    echo "[R36S] Patch no longer applies: inspect pinned PSXRecomp version." >&2
    exit 1
fi
