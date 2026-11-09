#!/usr/bin/env bash
# Source-only dArkOSRE Debian13 JNI/ELF/Bionic cloud test. No commercial guest.
set -euo pipefail
mkdir -p /results
cd /results
bin=/work/build/lenneth-probe
printf 'test\tstatus\tpeak_rss_kb\n' > suite.tsv
{
  printf 'key\tvalue\n'
  printf 'os\t%s\n' "$(. /etc/os-release; printf '%s' "$PRETTY_NAME")"
  printf 'arch\t%s\n' "$(uname -m)"
  printf 'kernel\t%s\n' "$(uname -r)"
  printf 'glibc\t%s\n' "$(getconf GNU_LIBC_VERSION)"
  printf 'gcc\t%s\n' "$(gcc -dumpfullversion)"
  printf 'SDL2\t%s\n' "$(sdl2-config --version)"
  printf 'memory_limit_bytes\t%s\n' "$(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo unknown)"
  printf 'swap_limit_bytes\t%s\n' "$(cat /sys/fs/cgroup/memory.swap.max 2>/dev/null || echo unknown)"
  printf 'cpu_quota\t%s\n' "$(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo unknown)"
  printf 'reference_cpu\t%s\n' '4x Cortex-A35 @ 1512 MHz'
  printf 'reference_ram_kib\t%s\n' '916800'
  printf 'reference_os\t%s\n' 'Debian 13 / glibc 2.41'
  printf 'reference_kernel\t%s\n' '4.4.189'
  printf 'reference_gpu\t%s\n' 'Mali-G31 Bifrost GBM'
  printf 'cloud_gpu\t%s\n' 'Mesa software llvmpipe, not Mali'
  printf 'hardware_performance_simulated\tNO\n'
  printf 'commercial_engine_executed\tNO\n'
} > environment.tsv
cat environment.tsv
grep -Eq $'^os\tDebian GNU/Linux 13' environment.tsv
grep -q $'^arch\taarch64' environment.tsv
grep -q $'^glibc\tglibc 2.41' environment.tsv
for name in threads fortify cxx fd unicode bionic-data ndk-dl \
  saveops stdio opensl logging phase17 locale18 phase19 phase20 phase21 \
  phase22 phase23 startup24 startup25 gles1-probe26 jni27 \
  gles1-context27 gles1-context28 gles1-render29 \
  jni-demand31 jni-contract32 jni-services33 pcm; do
    flag="--$name-selftest"
    echo "[debian13] $flag"
    if ! timeout 40 /usr/bin/time -f 'peak_rss_kb=%M elapsed=%e' \
          "$bin" "$flag" > "$name.log" 2>&1; then
        tail -n 45 "$name.log"
        printf '%s\tFAIL\tunknown\n' "$name" >> suite.tsv
        exit 41
    fi
    rss=$(grep -o 'peak_rss_kb=[0-9]*' "$name.log" | tail -n1 | cut -d= -f2 || true)
    printf '%s\tPASS\t%s\n' "$name" "$rss" >> suite.tsv
done
cc -shared -nostdlib -fPIC -O2 -fno-stack-protector -Wl,-z,relro \
  -o /tmp/phase30_partner.so /work/lenneth-r36s/tests/phase30_partner.c
cc -shared -nostdlib -fPIC -O2 -fno-stack-protector -Wl,-z,relro \
  -o /tmp/phase30_game.so /work/lenneth-r36s/tests/phase30_game.c
timeout 20 "$bin" --execute-lifecycle30 \
  /tmp/phase30_game.so /tmp/phase30_partner.so > lifecycle30.log
grep -q 'SYNTHETIC_LIFECYCLE=PASS' lifecycle30.log
timeout 20 "$bin" --jni-demand31 \
  /tmp/phase30_game.so /tmp/phase30_partner.so > inventory31.log
grep -q 'JNI_DEMAND=PASS' inventory31.log
if "$bin" --execute-lifecycle30 /etc/os-release /etc/os-release >/dev/null 2>&1; then
  echo 'ERROR: invalid ELF accepted' >&2
  exit 42
fi
if timeout 20 "$bin" --gles1-render29 > mesa-gles1.log 2>&1; then
  if grep -q 'GLES1_RASTER=PASS' mesa-gles1.log; then
    printf 'MESA_SOFTWARE_GLES1=PASS\n' > mesa-status.txt
  else
    printf 'MESA_SOFTWARE_GLES1=UNVERIFIED\n' > mesa-status.txt
  fi
else
  printf 'MESA_SOFTWARE_GLES1=UNAVAILABLE\n' > mesa-status.txt
fi
count=$(awk -F '\t' '$2=="PASS" {n++} END {print n+0}' suite.tsv)
test "$count" -eq 29
printf 'CLOUD_DEBIAN13_FINAL=PASS selftests=%s commercial_engine=NO\n' "$count" | tee result.txt
