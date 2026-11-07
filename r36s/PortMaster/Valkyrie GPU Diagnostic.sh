#!/bin/bash
# EGL/GLES hardware probe for dArkOSRE/ArkOS4Clone.
# Use firmware libs. NO glibc, EGL, or GBM replacement.
set +u
ports="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
game="$ports/valkyrierecomp"
log="$game/gles-gpu-probe.log"
mkdir -p "$game"
exec > "$log" 2>&1
echo "=== R36S GLES hardware probe: $(date -Iseconds 2>/dev/null || date) ==="
echo "System: $(uname -a)"
echo "glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || echo unknown)"
for folder in /opt/system/Tools/PortMaster /opt/tools/PortMaster \
    "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster" \
    /roms/ports/PortMaster /roms2/ports/PortMaster; do
    if [ -f "$folder/control.txt" ]; then
        echo "PortMaster: $folder"
        source "$folder/control.txt"
        if [ -n "${CFW_NAME:-}" ] && [ -f "$folder/mod_${CFW_NAME}.txt" ]; then
            source "$folder/mod_${CFW_NAME}.txt"
        fi
        if declare -F get_controls >/dev/null; then get_controls; fi
        break
    fi
done
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_VIDEO_FORCE_EGL="${SDL_VIDEO_FORCE_EGL:-1}"
echo "Video=${SDL_VIDEODRIVER}  EGL=${SDL_VIDEO_FORCE_EGL}"
if [ ! -x "$game/gles-gpu-probe" ]; then
    echo "ERROR: gles-gpu-probe executable missing from $game"
    exit 127
fi
cd "$game" || exit 1
./gles-gpu-probe
rc=$?
echo "GLES diagnostic exit code: $rc"
if declare -F pm_finish >/dev/null; then pm_finish; fi
echo "Log: $log"
exit "$rc"
