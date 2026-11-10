#!/bin/sh
# Lenneth vpl_nx R36S - hardware diagnostics. NEVER runs commercial game.
# This script deliberately uses POSIX sh + log-first fail-safe behavior.
set -u
umask 077
BASE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)" || exit 2
cd "$BASE" || exit 2
mkdir -p "$BASE/diagnosticos" 2>/dev/null || exit 2
echo "INICIO=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || echo sem-data)" > "$BASE/VPLNX_R36S_INICIO.log"
STAMP="$(date +%Y%m%d-%H%M%S 2>/dev/null || echo manual)"
D="$BASE/diagnosticos"
RUN="$D/execucao-$STAMP-$$"
mkdir -p "$RUN" || exit 2
LOG="$RUN/execucao.txt"
exec >> "$LOG" 2>&1
printf 'VPLNX_R36S_DIAGNOSTICO_V1=START\n'
printf 'base=%s\n' "$BASE"
printf 'COMMERCIAL_ENGINE=NOT_EXECUTED\nRMS=NOT_EXECUTED\n'
STATUS="FINISHED"
finish() {
    echo "FINAL=$STATUS" >> "$LOG"
    echo "COMMERCIAL_ENGINE=NOT_EXECUTED" >> "$LOG"
    printf 'STATUS=%s\nRUN=%s\n' "$STATUS" "$(basename "$RUN")" > "$RUN/resumo.txt"
    # Include only diagnostic text; never include .so files or game assets.
    tar -czf "$BASE/VPLNX_R36S_DIAGNOSTICOS.tar.gz" -C "$D" "$(basename "$RUN")" 2>>"$BASE/VPLNX_R36S_INICIO.log" || {
        cp "$LOG" "$BASE/VPLNX_R36S_ERRO_COLETA.log" 2>/dev/null || true
    }
    printf 'ULTIMA_EXECUCAO=%s\nSTATUS=%s\n' "$(basename "$RUN")" "$STATUS" >> "$BASE/VPLNX_R36S_INICIO.log"
}
trap 'finish' 0
trap 'STATUS=INTERRUPTED; exit 130' 2
trap 'STATUS=TERMINATED; exit 143' 15

printf '\n== HARDWARE ==\n'
uname -a || true
printf 'machine='; uname -m || true
printf 'video_devices='; ls /dev/dri 2>&1 || true
printf 'video_driver_environment=%s\n' "$(printenv SDL_VIDEODRIVER 2>/dev/null || echo automatico)"
printf 'audio_driver_environment=%s\n' "$(printenv SDL_AUDIODRIVER 2>/dev/null || echo automatico)"
printf '\n== SYSTEM ==\n'
sed -n '1,10p' /etc/os-release 2>/dev/null || true
awk '/MemTotal|MemAvailable|SwapTotal/ {print}' /proc/meminfo 2>/dev/null || true
printf '\n== RUNTIME ==\n'
command -v timeout || true
command -v ldd || true
command -v tar || true

BIN="$BASE/bin"
if [ ! -d "$BIN" ]; then
    STATUS="BINARIOS_AUSENTES"
    echo "BINARIOS_AUSENTES: extraia o ZIP de distribuicao completo em ports/"
    exit 0
fi
if ! command -v timeout >/dev/null 2>&1; then
    STATUS="TIMEOUT_AUSENTE"
    echo "TIMEOUT_AUSENTE: teste nativo foi pulado para evitar travamento"
    exit 0
fi

run_case() {
    NAME="$1"
    EXPECTED="$2"
    shift 2
    OUT="$RUN/$NAME.log"
    echo "== $NAME =="
    RC=0
    timeout 18 "$@" > "$OUT" 2>&1 || RC=$?
    if [ "$RC" -eq 0 ] && grep -Fq "$EXPECTED" "$OUT"; then
        RESULTADO="PASS"
    elif [ "$RC" -eq 0 ] && grep -Eq 'BIONIC_HOST_LINK=BLOCKED|NEXT_GATE=BLOCKED_' "$OUT"; then
        RESULTADO="BLOCKED"
    else
        RESULTADO="FALHA"
    fi
    echo "$NAME=$RESULTADO codigo=$RC"
    printf '%s\t%s\t%s\n' "$NAME" "$RESULTADO" "$RC" >> "$RUN/resultados.tsv"
    # Full diagnostics remain in tar.gz; avoid printing hundreds of imports.
    tail -n 7 "$OUT" 2>/dev/null || true
}
printf 'teste\tstatus\tcodigo\n' > "$RUN/resultados.tsv"
for f in vplnx-linux-platform-check vplnx-linux-audio-check vplnx-linux-jni-check vplnx-linux-host-check vplnx-linux-elf-check vplnx-bionic-pthread-check; do
    if [ -f "$BIN/$f" ]; then
        chmod +x "$BIN/$f" 2>/dev/null || true
        printf '\n%s:\n' "$f"
        if command -v ldd >/dev/null 2>&1; then ldd "$BIN/$f" || true; fi
    fi
done

if [ -x "$BIN/vplnx-bionic-pthread-check" ]; then
    run_case PTHREAD_BIONIC_ABI 'BIONIC_PTHREAD_ABI=PASS' "$BIN/vplnx-bionic-pthread-check"
else
    echo "PTHREAD_BIONIC_ABI=SKIP_SEM_EXECUTAVEL"
fi

if [ -x "$BIN/vplnx-linux-host-check" ]; then
    run_case HOST_SAFE 'BIONIC_HOST_SELFTEST=PASS' "$BIN/vplnx-linux-host-check" --host-selftest
else
    echo "HOST_SAFE=SKIP_SEM_EXECUTAVEL"
fi

# Platform check draws a blank 640x480 EGL surface for ~2 seconds.
# The GPU/driver printed here must identify hardware to confirm acceleration.
if [ -x "$BIN/vplnx-linux-platform-check" ]; then
    run_case GLES2_GPU 'VPLNX_LINUX_PLATFORM=PASS' "$BIN/vplnx-linux-platform-check"
    run_case GLES3_GPU 'VPLNX_LINUX_PLATFORM=PASS' "$BIN/vplnx-linux-platform-check" --es3
    # If SDL picked an unsuitable auto video driver, retry explicitly with
    # KMSDRM. Never spoof success if both attempts fail.
    if [ -e /dev/dri/card0 ] && ! grep -q 'VPLNX_LINUX_PLATFORM=PASS' "$RUN/GLES2_GPU.log"; then
        run_case GLES2_KMSDRM 'VPLNX_LINUX_PLATFORM=PASS' env SDL_VIDEODRIVER=kmsdrm "$BIN/vplnx-linux-platform-check"
    fi
    if [ -e /dev/dri/card0 ] && ! grep -q 'VPLNX_LINUX_PLATFORM=PASS' "$RUN/GLES3_GPU.log"; then
        run_case GLES3_KMSDRM 'VPLNX_LINUX_PLATFORM=PASS' env SDL_VIDEODRIVER=kmsdrm "$BIN/vplnx-linux-platform-check" --es3
    fi
else
    echo "GLES_GPU=SKIP_SEM_EXECUTAVEL"
fi

if [ -x "$BIN/vplnx-linux-audio-check" ]; then
    run_case OPENSL_AUDIO_FISICO 'VPLNX_OPENSL=PASS' "$BIN/vplnx-linux-audio-check"
    # Dummy fallback does NOT verify real speakers.
    if ! grep -q '^VPLNX_OPENSL=PASS' "$RUN/OPENSL_AUDIO_FISICO.log" 2>/dev/null; then
        run_case OPENSL_AUDIO_SIMULADO 'VPLNX_OPENSL=PASS' env SDL_AUDIODRIVER=dummy "$BIN/vplnx-linux-audio-check"
    fi
else
    echo "OPENSL_AUDIO=SKIP_SEM_EXECUTAVEL"
fi

# JNI test needs an original, tiny mock asset, never a game archive.
mkdir -p "$RUN/assets"
printf 'SYNTHJNI' > "$RUN/assets/sample.bin"
if [ -x "$BIN/vplnx-linux-jni-check" ]; then
    run_case JNI_ASSETS 'VPLNX_JNI_ABI=PASS' "$BIN/vplnx-linux-jni-check" "$RUN/assets"
else
    echo "JNI_ASSETS=SKIP_SEM_EXECUTAVEL"
fi
rm -f "$RUN/assets/sample.bin"
rmdir "$RUN/assets" 2>/dev/null || true

# Look only in known private locations, never traverse a full SD card.
# No .so is copied, hashed, packaged, loaded via dlopen, or executed.
PARENT="$(dirname "$BASE")"
GAME=""
PARTNER=""
for CAND in "$BASE/private" "$PARENT/lenneth-fase42" "$PARENT/lenneth-fase42/lib/arm64-v8a"; do
    if [ -f "$CAND/libMyGame.so" ] && [ -f "$CAND/libc++_shared.so" ]; then
        GAME="$CAND/libMyGame.so"
        PARTNER="$CAND/libc++_shared.so"
        break
    fi
done
if [ -n "$GAME" ] && [ -x "$BIN/vplnx-linux-host-check" ]; then
    echo "PRIVATE_LIBRARIES=FOUND (names only)"
    run_case ELF_REAL_IMPORTS 'BIONIC_HOST_LINK=PASS' "$BIN/vplnx-linux-host-check" "$GAME" "$PARTNER"
    if [ -x "$BIN/vplnx-linux-elf-check" ]; then
        run_case ELF_REAL_BASIC 'NEXT_GATE=READY_FOR_SEPARATE_JNI_ABI_TEST' "$BIN/vplnx-linux-elf-check" "$GAME" "$PARTNER"
    fi
else
    echo "PRIVATE_LIBRARIES=SKIPPED; expected private/ or ports/lenneth-fase42/"
fi

echo "DIAGNOSTIC_PACKAGE=$BASE/VPLNX_R36S_DIAGNOSTICOS.tar.gz"
echo "VPLNX_R36S_DIAGNOSTICO_V1=END"
