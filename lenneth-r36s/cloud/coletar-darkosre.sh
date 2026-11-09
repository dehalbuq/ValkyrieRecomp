#!/bin/sh
# dArkOSRE R36S cloud lab: read-only hardware metadata (MIT).
# No APK, private .so, firmware, secrets, IP, MAC or serial collected.
set -u
umask 077
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
OUT="$BASE/darkosre-r36s-perfil.zip"
test "$#" -ge 1 && OUT="$1"
WORK=$(mktemp -d /tmp/darkosre-r36s.XXXXXX) || exit 1
trap 'rm -rf "$WORK"' 0 1 2 3 15
printf 'key\tvalue\n' > "$WORK/profile.tsv"
p() { printf '%s\t%s\n' "$1" "$(printf '%s' "$2" | tr '\r\n\t' '   ' | cut -c1-200)" >> "$WORK/profile.tsv"; }
sf() { test -r "$1" && head -c 250 "$1" | tr '\000\r\n\t' '    ' || :; }
p arch "$(uname -m)"
p kernel.release "$(uname -r)"
p kernel.version "$(uname -v)"
p libc "$(getconf GNU_LIBC_VERSION 2>/dev/null || echo unknown)"
p cpu.online "$(sf /sys/devices/system/cpu/online)"
p board.model "$(sf /proc/device-tree/model)"
for k in MemTotal MemAvailable SwapTotal SwapFree CmaTotal; do
  v=$(awk -v k="$k:" '$1==k {print $2 " " $3;exit}' /proc/meminfo 2>/dev/null)
  p "memory.$k" "$v"
done
for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
  test -d "$cpu" || continue
  c=$(basename "$cpu")
  for k in scaling_governor scaling_cur_freq cpuinfo_max_freq cpuinfo_min_freq scaling_max_freq scaling_min_freq; do
    v=$(sf "$cpu/cpufreq/$k")
    test -n "$v" && p "$c.$k" "$v"
  done
done
{
  echo 'CPU architecture (filtered, no serial)'
  grep -E '^(Features|CPU implementer|CPU architecture|CPU variant|CPU part|CPU revision|Hardware|model name)[[:space:]]*:' /proc/cpuinfo 2>/dev/null | head -n 35
  echo 'GPU/DRM device names'
  for f in /dev/dri/card[0-9]* /dev/dri/renderD* /dev/mali*; do
    test -e "$f" && basename "$f"
  done
  echo 'Input names and capabilities (not IDs)'
  grep -E '^N: Name=|^B: (EV|ABS)=' /proc/bus/input/devices 2>/dev/null | head -n 45
  echo 'ALSA cards and PCM names'
  sed -n '1,18p' /proc/asound/cards 2>/dev/null
  sed -n '1,18p' /proc/asound/pcm 2>/dev/null
  echo 'Installed library SONAME records (no binaries)'
  if command -v ldconfig >/dev/null 2>&1; then
    ldconfig -p 2>/dev/null | grep -E 'lib(GLESv1_CM|GLESv2|EGL|SDL2|asound|mali)' | head -n 60
  fi
  echo 'Compiler and SDL2 versions'
  for tool in cc gcc clang cmake sdl2-config; do
    if command -v "$tool" >/dev/null 2>&1; then
      echo "$tool available"
    fi
  done
  command -v sdl2-config >/dev/null 2>&1 && sdl2-config --version
  echo 'Kernel options, filtered'
  if test -r /proc/config.gz && command -v zcat >/dev/null 2>&1; then
    zcat /proc/config.gz 2>/dev/null | grep -E '^CONFIG_(DRM|MALI|PANFROST|CGROUP|MEMCG|ZRAM|SND|ARM64)[A-Z0-9_]*=' | head -n 90
  fi
} > "$WORK/hardware.txt"
for log in "$BASE/lenneth-r36s.log" "$BASE/lenneth-r36s/lenneth-r36s.log"; do
  if test -r "$log"; then
    grep -E '^\[lenneth (video|audio|input)\] (vendor=|sdl_driver=|driver=|name=)' "$log" |
       tail -n 12 > "$WORK/graphics-audio.txt"
    break
  fi
done
cat > "$WORK/LEIA-ME.txt" <<'EOF'
PERFIL R36S: confira antes de enviar.
Nao contem APK, jogos, ROMs, drivers proprietarios, senhas, IP, MAC,
SSID, serial ou credenciais. Nenhum upload automatico.
A nuvem nao reproduz a GPU Mali nem o kernel do aparelho.
EOF
if command -v zip >/dev/null 2>&1; then
  (cd "$WORK" && zip -q "$OUT" ./*) || exit 2
else
  OUT="$BASE/darkosre-r36s-perfil.tar.gz"
  (cd "$WORK" && tar -czf "$OUT" ./*) || exit 2
fi
echo "COLETA=PASS arquivo=$OUT"
