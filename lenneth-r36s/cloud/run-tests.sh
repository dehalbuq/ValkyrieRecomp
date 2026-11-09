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
# Phase34: real JNI C header ABI; independently compiled synthetic JNI_OnLoad.
trap 'rm -f /results/lenneth-phase34-host /results/lenneth-phase34-fixture.so' EXIT
jni_header=$(find /usr/lib/jvm -type f -name jni.h -print -quit)
test -n "$jni_header" && test -f "$jni_header"
jni_include=$(dirname "$jni_header")
cc -std=c11 -O2 -Wall -Wextra -fPIC -shared -Wl,-z,relro \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase34-fixture.so /work/lenneth-r36s/cloud/phase34_guest.c
cc -std=c11 -O2 -Wall -Wextra -pthread \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase34-host /work/lenneth-r36s/cloud/phase34_host.c -ldl
timeout 20 /usr/bin/time -f 'peak_rss_kb=%M elapsed=%e' \
  /results/lenneth-phase34-host > phase34.log 2>&1 || {
    cat phase34.log
    printf 'phase34\tFAIL\tunknown\n' >> suite.tsv
    exit 46
  }
grep -q 'JNI_INTEGRATION=PASS' phase34.log
grep -q 'RegisterNatives=PASS' phase34.log
grep -q 'nativeInit_render=PASS' phase34.log
grep -q 'JNI_ONLOAD_REGISTER_NATIVES[[:space:]]*PASS' lenneth-jni-integration34.tsv
grep -q 'COMMERCIAL_ENGINE[[:space:]]*BLOCKED' lenneth-jni-integration34.tsv
rss34=$(grep -o 'peak_rss_kb=[0-9]*' phase34.log | tail -n1 | cut -d= -f2 || true)
test -n "$rss34" || rss34=unknown
printf 'phase34\tPASS\t%s\n' "$rss34" >> suite.tsv
# Phase35: synthetic DEX method-table and JNI flags, no external APK.
python3 -B /work/lenneth-r36s/cloud/dex_audit35.py \
  --selftest --output /results/lenneth-dex-contract35.tsv \
  > /results/phase35.log
grep -q 'PHASE35_DEX_AUDIT=PASS' /results/phase35.log
grep -q 'external_apk=NOT_PROVIDED' /results/phase35.log
grep -q 'getCocos2dxWritablePath.*DECLARED' /results/lenneth-dex-contract35.tsv
grep -q 'nativeFixtureRender.*DECLARED_NATIVE' /results/lenneth-dex-contract35.tsv
grep -q 'getAssets.*REFERENCE_ONLY' /results/lenneth-dex-contract35.tsv
grep -q 'GATE.*EXTERNAL_APK.*NOT_PROVIDED' /results/lenneth-dex-contract35.tsv
printf 'phase35\tPASS\tmetadata_only\n' >> suite.tsv
count=$(awk -F '\t' '$2=="PASS" {n++} END {print n+0}' suite.tsv)
test "$count" -eq 31
printf 'CLOUD_DEBIAN13_FINAL=PASS selftests=%s phase34=PASS phase35=PASS commercial_engine=NO\n' "$count" | tee result.txt
