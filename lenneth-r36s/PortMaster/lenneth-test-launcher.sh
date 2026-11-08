#!/bin/bash
# Lenneth R36S phase 32: ONE combined diagnostic run, compact reports.
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

echo "=== Lenneth R36S BATCH DIAGNOSTIC PHASE 32 $(date -Iseconds 2>/dev/null || date) ==="
echo "Kernel: $(uname -a)"
echo "glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
echo "[lenneth suite] Single run: private ARM64 ELF + GLES2 + portable libc + AAsset + Android log + JNI inventory + native video/audio/gamepad."
echo "[lenneth suite] This does NOT execute commercial game code."
echo "[lenneth suite] Experimental pthread guest-object adapters and extra libc/libm functions enabled in this batch."
echo "[lenneth suite] Additional bounded Bionic FORTIFY/errno adapters and synthetic self-test enabled."
echo "[lenneth suite] C++ lifecycle + independent synthetic ARM64 executable loader test enabled."
echo "[lenneth suite] Phase12: sandboxed POSIX fd, raw UTF-32 and process-local stack guard tests enabled."
echo "[lenneth suite] Phase13: allowlisted GLES/EGL guest dl* handles, save-only rename/mkdir/remove, and scalar wchar operations."
echo "[lenneth suite] Phase14: opaque FILE proxy; Phase22 resolves legacy __sF data separately."
echo "[lenneth suite] Phase15: OpenSL ES ABI remains synthetic (guest PCM not wired)."
echo "[lenneth suite] Phase16: native SDL2/ALSA PCM playback fixture retained."
echo "[lenneth suite] Phase17: sandboxed directory/time/text/logging ABI tests retained."
echo "[lenneth suite] Phase18: Bionic locale and incremental UTF-8 regression tests retained."
echo "[lenneth suite] Phase19: Bionic stat/fstat and sysconf regression tests retained."
echo "[lenneth suite] Phase20: sandboxed fdopen, priority and syscall regression tests retained."
echo "[lenneth suite] Phase21: guest-only ELF program headers and shell denial retained."
echo "[lenneth suite] Phase22: Bionic legacy __sF[3] data and historical jump ABI audit retained."
echo "[lenneth suite] Phase23: native AArch64 nonlocal jump tests retained."
echo "[lenneth suite] Phase24: read-only ELF startup audit retained."
echo "[lenneth suite] Phase25: constructor and direct GLES1 fixed-function import inventory retained."
echo "[lenneth suite] Phase26: native GLES1 library/export availability probe retained."
echo "[lenneth suite] Phase27: isolated JNI ABI and SDL/EGL GLES1 tests retained."
echo "[lenneth suite] Phase28: EGL ES1 dispatch diagnosis and validated standalone context retained."
echo "[lenneth suite] Phase29: native GLES1 fixed-function raster readback verified."
echo "[lenneth suite] Phase30: marker-verified mock constructors and mock JNI callbacks retained."
echo "[lenneth suite] Phase31: read-only complete JNI exports plus allowlisted Cocos Java class/method literal audit."
echo "[lenneth suite] Phase32: isolated C JNI class/method/string/exception ABI mock (commercial code not invoked)."

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
rm -f "$root/lenneth-suite-summary.txt" "$root/lenneth-blockers.tsv" "$root/lenneth-startup-gates.tsv" "$root/lenneth-phase25-startup-plan.tsv" "$root/lenneth-gles1-runtime26.tsv" "$root/lenneth-gles1-context27.tsv" "$root/lenneth-gles1-selection28.tsv" "$root/lenneth-gles1-raster29.tsv" "$root/lenneth-phase30-lifecycle.tsv" "$root/lenneth-jni-demand31.tsv"
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

# Phase27 probes the real GLES1 context in a fresh child process, after the
# normal SDL GLES2/ALSA/gamepad probe has shut down. A driver crash/hang must
# not stop us from delivering the previous six reports.
echo "[lenneth phase27] Starting optional isolated GLES1 1.x SDL/EGL context probe."
if command -v timeout >/dev/null 2>&1; then
    timeout 12 ./lenneth-probe --gles1-context27
    ctx27_rc=$?
else
    ./lenneth-probe --gles1-context27
    ctx27_rc=$?
fi
if [ ! -f "$root/lenneth-gles1-context27.tsv" ]; then
    printf 'provider\tgate\tstatus\tdetail\nhost\tEGL_ES1_CONTEXT\tUNAVAILABLE\tprobe_failed_or_timed_out_%s\n' "$ctx27_rc" > "$root/lenneth-gles1-context27.tsv"
fi
if grep -q 'GLES1_FIXED_FUNCTION[[:space:]]*PASS' "$root/lenneth-gles1-context27.tsv"; then
    echo "[lenneth phase27] R36S_ES1_CONTEXT=PASS"
    echo "Native GLES1 EGL 1.x context on device: PASS" >> "$root/lenneth-suite-summary.txt"
else
    echo "[lenneth phase27] R36S_ES1_CONTEXT=UNAVAILABLE (nonfatal, exit=$ctx27_rc)"
    echo "Native GLES1 EGL 1.x context on device: UNAVAILABLE (exit=$ctx27_rc)" >> "$root/lenneth-suite-summary.txt"
fi

# Phase28 runs after phase27 in a separate process, so if a graphics driver
# refuses or crashes on explicit ES1 pbuffer creation, diagnostic reports stay
# intact. This does not execute game code.
echo "[lenneth phase28] Testing EGL version selected by SDL and an explicit ES1 pbuffer."
if command -v timeout >/dev/null 2>&1; then
    timeout 14 ./lenneth-probe --gles1-context28
    ctx28_rc=$?
else
    ./lenneth-probe --gles1-context28
    ctx28_rc=$?
fi
if [ ! -f "$root/lenneth-gles1-selection28.tsv" ]; then
    printf 'provider\tgate\tstatus\tdetail\nnative\tFINAL_ES1_VALIDATION\tUNVERIFIED\tprobe_crashed_or_timed_out_%s\n' "$ctx28_rc" > "$root/lenneth-gles1-selection28.tsv"
fi
if grep -q 'FINAL_ES1_VALIDATION[[:space:]]*PASS' "$root/lenneth-gles1-selection28.tsv"; then
    echo "[lenneth phase28] R36S_ES1_SELECTION=PASS"
    echo "EGL ES1 context selection and dispatch: PASS" >> "$root/lenneth-suite-summary.txt"
else
    echo "[lenneth phase28] R36S_ES1_SELECTION=UNVERIFIED (nonfatal, exit=$ctx28_rc)"
    echo "EGL ES1 context selection and dispatch: UNVERIFIED (exit=$ctx28_rc)" >> "$root/lenneth-suite-summary.txt"
fi

# Phase29 verifies actual ES1.1 fixed-function rasterization and readback on a
# tiny EGL pbuffer. It is a separate child process, NOT a game launcher.
echo "[lenneth phase29] Testing native GLES1 1.1 fixed-function raster via EGL function pointers."
if command -v timeout >/dev/null 2>&1; then
    timeout 13 ./lenneth-probe --gles1-render29
    render29_rc=$?
else
    ./lenneth-probe --gles1-render29
    render29_rc=$?
fi
if [ ! -f "$root/lenneth-gles1-raster29.tsv" ]; then
    printf 'provider\tgate\tstatus\tdetail\nnative\tFINAL_ES1_RASTER\tUNVERIFIED\tprobe_crashed_or_timed_out_%s\n' "$render29_rc" > "$root/lenneth-gles1-raster29.tsv"
fi
if grep -q 'FINAL_ES1_RASTER[[:space:]]*PASS' "$root/lenneth-gles1-raster29.tsv"; then
    echo "[lenneth phase29] R36S_ES1_RASTER=PASS"
    echo "Native GLES1 fixed-function triangle and clear pixel readback: PASS" >> "$root/lenneth-suite-summary.txt"
else
    echo "[lenneth phase29] R36S_ES1_RASTER=UNVERIFIED (nonfatal, exit=$render29_rc)"
    echo "Native GLES1 fixed-function triangle and clear pixel readback: UNVERIFIED (exit=$render29_rc)" >> "$root/lenneth-suite-summary.txt"
fi

# Phase30 is an actual native ARM64 guest-loader startup replay for two
# tiny independently authored mock ELF files bundled under mock/. No real
# libMyGame.so, JNI_OnLoad, Cocos nativeInit/Render or its 196 commercial
# constructors are loaded into executable memory or called. Refuse absent
# fixtures rather than accidentally using the game's libraries.
echo "[lenneth phase30] Running marker-verified synthetic AArch64 startup and JNI callbacks."
lifecycle30_rc=30
if [ -f "./mock/startup30_game.so" ] && [ -f "./mock/startup30_partner.so" ]; then
    if command -v timeout >/dev/null 2>&1; then
        timeout 10 ./lenneth-probe --execute-lifecycle30             "./mock/startup30_game.so" "./mock/startup30_partner.so"
        lifecycle30_rc=$?
    else
        ./lenneth-probe --execute-lifecycle30             "./mock/startup30_game.so" "./mock/startup30_partner.so"
        lifecycle30_rc=$?
    fi
else
    echo "[lenneth phase30] Synthetic fixture missing; no substitute permitted."
fi
if [ ! -f "$root/lenneth-phase30-lifecycle.tsv" ]; then
    printf 'component\tgate\tstatus\tdetail\nstartup\tSYNTHETIC_LIFECYCLE\tFAIL\tfixture_missing_or_failed_%s\nreal_game\tCOMMERCIAL_ENGINE\tBLOCKED\tnever_executed\n' "$lifecycle30_rc" > "$root/lenneth-phase30-lifecycle.tsv"
fi
if [ "$lifecycle30_rc" -eq 0 ] && grep -q 'JNI_ONLOAD_INIT_RENDER[[:space:]]*PASS' "$root/lenneth-phase30-lifecycle.tsv"; then
    echo "[lenneth phase30] R36S_SYNTHETIC_STARTUP=PASS"
    if [ -f "$root/lenneth-suite-summary.txt" ]; then
        echo "Synthetic ARM64 dependency constructors + JNI startup: PASS (commercial NOT executed)" >> "$root/lenneth-suite-summary.txt"
    fi
else
    echo "[lenneth phase30] R36S_SYNTHETIC_STARTUP=FAIL (exit=$lifecycle30_rc)"
    if [ -f "$root/lenneth-suite-summary.txt" ]; then
        echo "Synthetic ARM64 dependency constructors + JNI startup: FAIL (exit=$lifecycle30_rc)" >> "$root/lenneth-suite-summary.txt"
    fi
    if [ "$rc" -eq 0 ]; then rc=30; fi
fi

# Package only text reports. Never include libraries or game assets.
if [ -f "$root/lenneth-suite-summary.txt" ] && [ -f "$root/lenneth-blockers.tsv" ] && [ -f "$root/lenneth-startup-gates.tsv" ] && [ -f "$root/lenneth-phase25-startup-plan.tsv" ] && [ -f "$root/lenneth-gles1-runtime26.tsv" ] && [ -f "$root/lenneth-gles1-context27.tsv" ] && [ -f "$root/lenneth-gles1-selection28.tsv" ] && [ -f "$root/lenneth-gles1-raster29.tsv" ] && [ -f "$root/lenneth-phase30-lifecycle.tsv" ] && [ -f "$root/lenneth-jni-demand31.tsv" ] && [ -f "$root/lenneth-jni-services32.tsv" ]; then
    if command -v zip >/dev/null 2>&1; then
        (cd "$root" && zip -q -j "lenneth-r36s-diagnostics.zip" \
          "lenneth-r36s.log" "lenneth-suite-summary.txt" "lenneth-blockers.tsv" "lenneth-startup-gates.tsv" "lenneth-phase25-startup-plan.tsv" "lenneth-gles1-runtime26.tsv" "lenneth-gles1-context27.tsv" "lenneth-gles1-selection28.tsv" "lenneth-gles1-raster29.tsv" "lenneth-phase30-lifecycle.tsv" "lenneth-jni-demand31.tsv" "lenneth-jni-services32.tsv") \
          || echo "[lenneth suite] zip report packaging failed"
        echo "[lenneth suite] Reports: lenneth-r36s-diagnostics.zip"
    elif command -v tar >/dev/null 2>&1; then
        (cd "$root" && tar -czf "lenneth-r36s-diagnostics.tar.gz" \
          "lenneth-r36s.log" "lenneth-suite-summary.txt" "lenneth-blockers.tsv" "lenneth-startup-gates.tsv" "lenneth-phase25-startup-plan.tsv" "lenneth-gles1-runtime26.tsv" "lenneth-gles1-context27.tsv" "lenneth-gles1-selection28.tsv" "lenneth-gles1-raster29.tsv" "lenneth-phase30-lifecycle.tsv" "lenneth-jni-demand31.tsv" "lenneth-jni-services32.tsv") \
          || echo "[lenneth suite] tar report packaging failed"
        echo "[lenneth suite] Reports: lenneth-r36s-diagnostics.tar.gz"
    fi
fi
echo "[lenneth suite] Exit code=$rc. Text results: lenneth-r36s.log, lenneth-suite-summary.txt, lenneth-blockers.tsv, lenneth-startup-gates.tsv, lenneth-phase25-startup-plan.tsv, lenneth-gles1-runtime26.tsv, lenneth-gles1-context27.tsv, lenneth-gles1-selection28.tsv, lenneth-gles1-raster29.tsv, lenneth-phase30-lifecycle.tsv, lenneth-jni-demand31.tsv, lenneth-jni-services32.tsv"
if declare -F pm_finish >/dev/null; then pm_finish; fi
exit "$rc"
