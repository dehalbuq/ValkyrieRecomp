#!/bin/bash
# Valkyrie Profile native PSXRecomp / R36S / DarkOSRE (experimental).
# SDL2 GLES2 *presentation* is GPU accelerated when supported; the PS1 GPU
# rasterizer is still CPU/software. No unvalidated OpenGL-3 or Vulkan claims.
set +u
here="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
cd "$here" || exit 1
log="$here/valkyrie-r36s.log"
if ! ( : >> "$log" ) 2>/dev/null; then log="/tmp/valkyrie-r36s.log"; fi
exec >> "$log" 2>&1

echo "=== ValkyrieRecomp R36S $(date -Iseconds 2>/dev/null || date) ==="
echo "Firmware: $(uname -a); glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || echo unknown)"

# Match the working Killer Instinct R36S approach: PortMaster knows firmware
# SDL/KMSDRM libraries and its SDL_GAMECONTROLLERCONFIG for the built-in pad.
controlfolder=''
for folder in /opt/system/Tools/PortMaster /opt/tools/PortMaster \
    "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster" \
    /roms/ports/PortMaster /roms2/ports/PortMaster; do
    if [ -f "$folder/control.txt" ]; then controlfolder="$folder"; break; fi
done
if [ -n "$controlfolder" ]; then
    echo "[R36S] PortMaster controls: $controlfolder"
    # Older PortMaster shell files are Bash; never run under /bin/sh.
    source "$controlfolder/control.txt"
    if [ -n "${CFW_NAME:-}" ] && [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
        source "$controlfolder/mod_${CFW_NAME}.txt"
    fi
    if declare -F get_controls >/dev/null; then get_controls; fi
else
    echo "[R36S] PortMaster control.txt not found; using firmware SDL defaults."
fi
if [ -n "${sdl_controllerconfig:-}" ] && [ -z "${SDL_GAMECONTROLLERCONFIG:-}" ]; then
    export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
    echo "[R36S] Loaded firmware controller mappings."
fi
if [ -z "${SDL_VIDEODRIVER:-}" ]; then export SDL_VIDEODRIVER=kmsdrm; fi
if [ -z "${SDL_RENDER_DRIVER:-}" ]; then export SDL_RENDER_DRIVER=opengles2; fi
if [ -z "${SDL_VIDEO_FORCE_EGL:-}" ]; then export SDL_VIDEO_FORCE_EGL=1; fi
export SDL_RENDER_SCALE_QUALITY=0
# Linux handheld generally exposes hardware audio through ALSA. Users can
# override this to pulseaudio/pipewire by exporting SDL_AUDIODRIVER.
if [ -z "${SDL_AUDIODRIVER:-}" ]; then export SDL_AUDIODRIVER=alsa; fi
# Use direct SDL audio queue by default for this low-power port; users can
# A/B the resampling/DRC path with PSXRECOMP_AUDIO_LEGACY=0.
export PSXRECOMP_AUDIO_LEGACY="${PSXRECOMP_AUDIO_LEGACY:-1}"
# Physical pad takes priority; the fallback merge keeps keyboard usable too.
export PSX_DEV_INPUT="${PSX_DEV_INPUT:-1}"
# Audited PSXRecomp loop-shape idle skip. Conserves guest cycles and device
# scheduling; may improve CPU-bound BIOS/game spin loops. No unsafe counter
# decrement extension without game-specific sites. A/B with IDLE_SKIP=0.
export PSX_IDLE_SKIP="${VALKYRIE_IDLE_SKIP:-1}"
export PSX_IDLE_SKIP_EXT=0
echo "[R36S cpu] PSX_IDLE_SKIP=$PSX_IDLE_SKIP, PSX_IDLE_SKIP_EXT=0"
export PSX_NO_LAUNCHER=1
echo "[R36S] SDL_VIDEO=${SDL_VIDEODRIVER:-auto}, SDL_RENDER=${SDL_RENDER_DRIVER:-auto}, SDL_AUDIO=${SDL_AUDIODRIVER:-auto}, AUDIO_LEGACY=$PSXRECOMP_AUDIO_LEGACY"
echo "[R36S] Exit: hold START+SELECT about 2.5s, then A=exit or B=resume. F10 on keyboard."

aspect="${VALKYRIE_ASPECT:-4:3}"
case "$aspect" in 4:3|16:9) ;; *) echo "[ERROR] invalid aspect $aspect"; exit 2;; esac
disc_num="${VALKYRIE_DISC:-1}"
case "$disc_num" in 1|2) ;; *) echo "[ERROR] invalid disc $disc_num"; exit 2;; esac
disc="disc/Disc${disc_num}/Valkyrie Profile (USA) (Disc ${disc_num}).cue"
bin="${VALKYRIE_BIN:-./ValkyrieRecomp}"
if [ ! -x "$bin" ]; then echo "[ERROR] missing executable: $bin"; exit 127; fi
if [ ! -f game.toml ]; then echo "[ERROR] missing game.toml"; exit 1; fi
if [ ! -f "$disc" ]; then echo "[ERROR] missing disc: $disc"; exit 1; fi

# Config stays local; don't change desktop config or generated private game C.
# Avoid high-resolution supersampling / blending on 1GB R36S hardware.
if ! awk -v aspect="$aspect" '
    /^\[video\][[:space:]]*$/ {
        in_video=1; seen_video=1; print
        print "renderer = \"software\""
        print "aspect_ratio = \"" aspect "\""
        print "supersampling = 1"
        print "window_width = 640"
        print "antialiasing = false"
        next
    }
    /^\[/ { in_video=0 }
    in_video && /^[[:space:]]*(renderer|aspect_ratio|supersampling|window_width|antialiasing)[[:space:]]*=/ { next }
    { print }
    END { if (!seen_video) exit 2 }
' game.toml > game-r36s.local.toml; then
    echo "[ERROR] cannot generate local game.toml"; exit 1
fi
mkdir -p saves
export LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper "$bin"; fi
echo "[R36S] Starting: $bin --disc '$disc'"
"$bin" --game game-r36s.local.toml --disc "$disc" --no-launcher --renderer software
rc=$?
echo "[R36S] Game exit status: $rc"
if declare -F pm_finish >/dev/null; then pm_finish; fi
exit "$rc"
