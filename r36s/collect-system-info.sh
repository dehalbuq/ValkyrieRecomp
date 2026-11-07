#!/bin/sh
# Collect minimum compatibility details from an R36S (no personal files).
set -u
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 1
OUT="$HERE/r36s-system-info.txt"
if ! ( : > "$OUT" ) 2>/dev/null; then
    OUT="/tmp/r36s-system-info.txt"
fi
{
    echo "=== R36S / DarkOSRE system diagnostics ==="
    date 2>/dev/null || :
    echo "Architecture: $(uname -m 2>/dev/null)"
    echo "Bits: $(getconf LONG_BIT 2>/dev/null || echo unknown)"
    echo "GLIBC: $(getconf GNU_LIBC_VERSION 2>/dev/null || echo unavailable)"
    echo "--- OS ---"
    if [ -r /etc/os-release ]; then cat /etc/os-release; fi
    echo "--- Linux loader / libs ---"
    for f in /lib/ld-linux-aarch64.so.1 /lib/aarch64-linux-gnu/ld-linux-aarch64.so.1 \
             /usr/lib/libSDL2-2.0.so.0 /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0; do
        if [ -e "$f" ]; then ls -l "$f"; fi
    done
    echo "--- SDL2 ---"
    if command -v sdl2-config >/dev/null 2>&1; then
        sdl2-config --version 2>/dev/null || :
    else echo "sdl2-config unavailable"; fi
    echo "--- Runtime and kernel ---"
    uname -a 2>/dev/null || :
    echo "--- Executable, if any ---"
    if [ -f "$HERE/ValkyrieRecomp" ]; then
        if command -v file >/dev/null 2>&1; then file "$HERE/ValkyrieRecomp"; fi
        if command -v readelf >/dev/null 2>&1; then
            readelf -l "$HERE/ValkyrieRecomp" | grep -E 'interpreter' || :
        fi
        if command -v ldd >/dev/null 2>&1; then
            ldd "$HERE/ValkyrieRecomp" 2>&1 || :
        fi
    else echo "ValkyrieRecomp executable not present"; fi
} > "$OUT" 2>&1
echo "Diagnostics saved: $OUT"
