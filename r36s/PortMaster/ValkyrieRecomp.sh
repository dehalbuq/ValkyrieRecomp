#!/usr/bin/env bash
# Experimental standalone Ports-menu shortcut.
# Copy this file to /roms/ports/ValkyrieRecomp.sh, next to valkyrierecomp/.
set -euo pipefail

ports_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
game_dir="$ports_dir/valkyrierecomp"
launcher="$game_dir/ValkyrieRecomp.sh"

if [[ ! -f "$launcher" ]]; then
    echo "[ValkyrieRecomp] Missing inner launcher: $launcher" >&2
    exit 1
fi

# Execute through Bash even when the ROM partition does not preserve +x.
exec bash "$launcher" "$@"
