#!/usr/bin/env bash
# Apply narrow build-compatibility patches to the pinned PSXRecomp checkout.
# Keep the submodule pin unchanged; never edit or distribute game data.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
FRAMEWORK="$ROOT/psxrecomp"

if [ ! -f "$FRAMEWORK/runtime/src/overlay_loader.c" ]; then
    echo "[R36S] Missing PSXRecomp sources; initialize submodules first." >&2
    exit 1
fi

for PATCH in \
    "$ROOT/r36s/patches/psxrecomp-overlay-loader-label.patch" \
    "$ROOT/r36s/patches/psxrecomp-no-desktop-gl-link.patch" \
    "$ROOT/r36s/patches/psxrecomp-fntrace-c-linkage.patch"; do
    [ -f "$PATCH" ] || { echo "[R36S] Patch missing: $PATCH" >&2; exit 1; }
    if git -C "$FRAMEWORK" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
        echo "[R36S] Patch already applied: $(basename "$PATCH")"
    elif git -C "$FRAMEWORK" apply --check "$PATCH"; then
        git -C "$FRAMEWORK" apply "$PATCH"
        echo "[R36S] Applied: $(basename "$PATCH")"
    else
        echo "[R36S] Patch does not apply; inspect PSXRecomp pin: $PATCH" >&2
        exit 1
    fi
done
