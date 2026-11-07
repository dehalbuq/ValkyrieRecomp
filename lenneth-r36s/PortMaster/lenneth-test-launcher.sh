#!/bin/bash
# Lenneth R36S phase 15: ONE combined diagnostic run, compact reports.
# No game execution. Proprietary .so and data are NEVER packaged or uploaded.
set +u
root="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
cd "$root" || exit 1

log="$root/lenneth-r36s.log"
if ! ( : >> "$log" ) 2>/dev/null; then log="/tmp/lenneth-r36s.log"; fi
# Previous sessions were appended to the same log (over 100 KB).
# Keep the immediately previous one as a backup and start a clean file.
if [ -s "$log" ]; then cp -f "$log" "$root/lenneth-r36s.previous.log" 2>/dev/null || true; fi
exec > "$log" 2>&1

echo "=== Lenneth R36S BATCH DIAGNOSTIC PHASE 15 $(date -Iseconds 2>/dev/null || date) ==="
echo "Kernel: $(uname -a)"
echo "glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
echo "[lenneth suite] Single run: private ARM64 ELF + GLES2 + portable libc + AAsset + Android log + JNI inventory + native video/audio/gamepad."
echo "[lenneth suite] This does NOT execute commercial game code."
echo "[lenneth suite] Experimental pthread guest-object adapters and extra libc/libm functions enabled in this batch."
echo "[lenneth suite] Additional bounded Bionic FORTIFY/errno adapters and synthetic self-test enabled."
echo "[lenneth suite] C++ lifecycle + independent synthetic ARM64 executable loader test enabled."
echo "[lenneth suite] Phase12: sandboxed POSIX fd, raw UTF-32 and process-local stack guard tests enabled."
echo "[lenneth suite] Phase13: allowlisted GLES/EGL guest dl* handles, save-only rename/mkdir/remove, and scalar wchar operations."
echo "[lenneth suite] Phase14: opaque FILE proxy diagnostics; legacy Bionic __sF remains unresolved."
echo "[lenneth suite] Phase15: typed OpenSL ES engine/interface/queue ABI fixture; PCM audio backend is NOT implemented."

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
echo "SDL video=$SDL_VIDEODRIVER audio=$SDL_AUDIODRIVER"

if [ ! -x ./lenneth-probe ]; then
    echo "ERROR: lenneth-probe executable missing"
    exit 127
fi

game_so=""
cxx_so=""
for candidate in libMyGame.so lib/arm64-v8a/libMyGame.so; do
    if [ -f "$candidate" ]; then game_so="$candidate"; break; fi
done
for candidate in libc++_shared.so lib/arm64-v8a/libc++_shared.so; do
    if [ -f "$candidate" ]; then cxx_so="$candidate"; break; fi
done

if [ ! -d assets ]; then
    mkdir -p assets 2>/dev/null || echo "[lenneth asset] cannot create optional empty assets folder"
fi
if [ ! -d saves ]; then
    mkdir -m 700 -p saves 2>/dev/null || echo "[lenneth fd] cannot create app-private saves directory"
fi
rm -f "$root/lenneth-suite-summary.txt" "$root/lenneth-blockers.tsv"
echo "[lenneth synthetic] Testing explicitly marked original synthetic ARM64 modules."
synthetic_rc=9
if [ -f mock/synthetic_guest.so ] && [ -f mock/synthetic_partner.so ]; then
    ./lenneth-probe --execute-synthetic mock/synthetic_guest.so mock/synthetic_partner.so
    synthetic_rc=$?
else
    echo "[lenneth synthetic] fixture files missing"
fi
if [ "$synthetic_rc" -eq 0 ]; then
    echo "[lenneth synthetic] R36S_EXECUTION=PASS commercial_engine=NOT_EXECUTED"
else
    echo "[lenneth synthetic] R36S_EXECUTION=FAIL code=$synthetic_rc commercial_engine=NOT_EXECUTED"
fi


if [ -n "$game_so" ] && [ -n "$cxx_so" ]; then
    if [ "$LENNETH_FULL_DIAG" = 1 ]; then
        echo "Optional verbose legacy audit enabled"
        for so in "$game_so" "$cxx_so"; do
            ./lenneth-probe --inspect "$so"
            ./lenneth-probe --preflight "$so"
        done
        ./lenneth-probe --link-plan "$game_so" "$cxx_so"
        ./lenneth-probe --host-plan "$game_so" "$cxx_so"
        ./lenneth-probe --portable-plan "$game_so" "$cxx_so"
        ./lenneth-probe --asset-plan "$game_so" "$cxx_so"
        ./lenneth-probe --logging-plan "$game_so" "$cxx_so"
    fi
    echo "[lenneth suite] Running all default tests in one process."
    ./lenneth-probe --batch "$game_so" "$cxx_so"
    rc=$?
else
    echo "[lenneth suite] Cannot link private Android libraries: libMyGame.so or libc++_shared.so missing"
    ./lenneth-probe --video --seconds 15
    rc=10
fi
if [ -f "$root/lenneth-suite-summary.txt" ]; then
    if [ "$synthetic_rc" -eq 0 ]; then
        echo "Synthetic ARM64 ELF execution on R36S: PASS" >> "$root/lenneth-suite-summary.txt"
    else
        echo "Synthetic ARM64 ELF execution on R36S: FAIL ($synthetic_rc)" >> "$root/lenneth-suite-summary.txt"
    fi
fi
if [ "$synthetic_rc" -ne 0 ] && [ "$rc" -eq 0 ]; then rc=13; fi

# Package only three text reports. Never include libraries or game assets.
if [ -f "$root/lenneth-suite-summary.txt" ] && [ -f "$root/lenneth-blockers.tsv" ]; then
    if command -v zip >/dev/null 2>&1; then
        (cd "$root" && zip -q -j "lenneth-r36s-diagnostics.zip" \
          "lenneth-r36s.log" "lenneth-suite-summary.txt" "lenneth-blockers.tsv") \
          || echo "[lenneth suite] zip report packaging failed"
        echo "[lenneth suite] Reports: lenneth-r36s-diagnostics.zip"
    elif command -v tar >/dev/null 2>&1; then
        (cd "$root" && tar -czf "lenneth-r36s-diagnostics.tar.gz" \
          "lenneth-r36s.log" "lenneth-suite-summary.txt" "lenneth-blockers.tsv") \
          || echo "[lenneth suite] tar report packaging failed"
        echo "[lenneth suite] Reports: lenneth-r36s-diagnostics.tar.gz"
    fi
fi
echo "[lenneth suite] Exit code=$rc. Text results: lenneth-r36s.log, lenneth-suite-summary.txt, lenneth-blockers.tsv"
if declare -F pm_finish >/dev/null; then pm_finish; fi
exit "$rc"
