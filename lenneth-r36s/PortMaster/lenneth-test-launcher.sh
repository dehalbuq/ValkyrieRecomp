#!/bin/bash
# Native Lenneth ARM64 compatibility BOOTSTRAP, not the commercial game.
set +u
root="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
cd "$root" || exit 1
log="$root/lenneth-r36s.log"
if ! ( : >> "$log" ) 2>/dev/null; then log="/tmp/lenneth-r36s.log"; fi
exec >> "$log" 2>&1
echo "=== Lenneth Android R36S PHASE 1 $(date -Iseconds 2>/dev/null || date) ==="
echo "Kernel: $(uname -a)"
echo "glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
echo "This binary is a native EGL/GLES2/gamepad/audio/ELF diagnostic, NOT the commercial game."
for pm in /opt/system/Tools/PortMaster /opt/tools/PortMaster \
    "$HOME/.local/share/PortMaster" /roms/ports/PortMaster /roms2/ports/PortMaster; do
    if [ -f "$pm/control.txt" ]; then
        echo "PortMaster: $pm"
        source "$pm/control.txt"
        if [ -n "$CFW_NAME" ] && [ -f "$pm/mod_$CFW_NAME.txt" ]; then
            source "$pm/mod_$CFW_NAME.txt"
        fi
        if declare -F get_controls >/dev/null; then get_controls; fi
        break
    fi
done
if [ -n "$sdl_controllerconfig" ] && [ -z "$SDL_GAMECONTROLLERCONFIG" ]; then
    export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi
if [ -z "$SDL_VIDEODRIVER" ]; then export SDL_VIDEODRIVER=kmsdrm; fi
if [ -z "$SDL_VIDEO_FORCE_EGL" ]; then export SDL_VIDEO_FORCE_EGL=1; fi
if [ -z "$SDL_AUDIODRIVER" ]; then export SDL_AUDIODRIVER=alsa; fi
export SDL_RENDER_SCALE_QUALITY=0
echo "SDL: video=$SDL_VIDEODRIVER audio=$SDL_AUDIODRIVER"
if [ ! -x ./lenneth-probe ]; then
    echo "ERROR: lenneth-probe executable missing"
    exit 127
fi
for so in libMyGame.so lib/arm64-v8a/libMyGame.so libc++_shared.so lib/arm64-v8a/libc++_shared.so libRMS.so lib/arm64-v8a/libRMS.so; do
    if [ -f "$so" ]; then
        echo "Native ELF metadata audit: $so"
        ./lenneth-probe --inspect "$so" || echo "WARNING: ELF audit failed for $so"
        echo "ELF mapped load / relative relocations: $so"
        ./lenneth-probe --preflight "$so" || echo "WARNING: read-only ELF mapping preflight failed for $so"
    fi
done
echo "Entering GLES2 hardware test (automatically quits after 30 seconds)."
echo "Exit earlier by START+SELECT held 2.5s, then A; or press F10."
./lenneth-probe --video --seconds 30
rc=$?
echo "Result: $rc. Log: $log"
if declare -F pm_finish >/dev/null; then pm_finish; fi
exit "$rc"
