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
# Phase36: independently authored synthetic DEX/export join, no real APK in CI.
python3 -B /work/lenneth-r36s/cloud/jni_crosscheck36.py \
  --selftest --output /results/lenneth-jni-join36.tsv \
  > /results/phase36.log
grep -q 'PHASE36_JNI_JOIN=PASS' /results/phase36.log
grep -q 'match_short=PASS match_long=PASS' /results/phase36.log
grep -q 'overloads=FAIL_CLOSED' /results/phase36.log
grep -q 'fixture_only=YES commercial_engine=NO' /results/phase36.log
grep -q 'AMBIGUOUS_OVERLOAD' /results/lenneth-jni-join36.tsv
grep -q 'NOT_EXPORTED_BY_LIBMYGAME' /results/lenneth-jni-join36.tsv
printf 'phase36\tPASS\tmetadata_only\n' >> suite.tsv
# Phase37: synthetic AArch64 JNI ELF with DEX/ELF-confirmed entrypoints.
cc -std=c11 -O2 -Wall -Wextra -fPIC -shared -Wl,-z,relro \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase37-fixture.so /work/lenneth-r36s/cloud/phase37_guest.c
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase37-host /work/lenneth-r36s/cloud/phase37_host.c -ldl
readelf -h /results/lenneth-phase37-fixture.so | grep -q 'Machine:.*AArch64'
nm -D --defined-only /results/lenneth-phase37-fixture.so > /results/phase37-exports.txt
while IFS=$'\t' read -r class method signature export provenance; do
  [ "$class" = class ] && continue
  [ "$provenance" = DEX35_ELF31_CONFIRMED ] || { echo "ERROR: contract provenance";exit 47; }
  grep -q " $export$" /results/phase37-exports.txt || { echo "ERROR: missing $export";exit 47; }
done < /work/lenneth-r36s/cloud/phase37_contract.tsv
grep -q ' JNI_OnLoad$' /results/phase37-exports.txt
timeout 20 /usr/bin/time -f 'peak_rss_kb=%M elapsed=%e' \
  /results/lenneth-phase37-host > /results/phase37.log 2>&1 || {
  cat /results/phase37.log
  printf 'phase37\tFAIL\tunknown\n' >> suite.tsv
  exit 47
}
grep -q 'PHASE37_JNI_LIFECYCLE=PASS' /results/phase37.log
grep -q 'negative=PASS' /results/phase37.log
grep -q 'globals_released=2' /results/phase37.log
grep -q 'INIT_RENDER_PAUSE_RESUME[[:space:]]*PASS' /results/lenneth-jni-lifecycle37.tsv
grep -q 'NO_REF_LEAK_AFTER_RESET[[:space:]]*PASS' /results/lenneth-jni-lifecycle37.tsv
grep -q 'COMMERCIAL_CONSTRUCTORS_AND_JNI[[:space:]]*BLOCKED' /results/lenneth-jni-lifecycle37.tsv
rss37=$(grep -o 'peak_rss_kb=[0-9]*' /results/phase37.log | tail -n1 | cut -d= -f2 || true)
if [ -z "$rss37" ]; then rss37=unknown; fi
printf 'phase37\tPASS\t%s\n' "$rss37" >> suite.tsv
rm -f /results/lenneth-phase37-fixture.so /results/lenneth-phase37-host
# Phase38: synthetic JNI/NDK AAssetManager bridge, no original game data.
cc -std=c11 -O2 -Wall -Wextra -fPIC -shared -Wl,-z,relro \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase38-fixture.so /work/lenneth-r36s/cloud/phase38_guest.c
cc -std=c11 -O2 -Wall -Wextra -rdynamic \
  -I "$jni_include" -I "$jni_include/linux" \
  -o /results/lenneth-phase38-host /work/lenneth-r36s/cloud/phase38_host.c -ldl
readelf -h /results/lenneth-phase38-fixture.so | grep -q 'Machine:.*AArch64'
readelf -h /results/lenneth-phase38-host | grep -q 'Machine:.*AArch64'
nm -D /results/lenneth-phase38-fixture.so > /results/phase38-guest-symbols.txt
nm -D /results/lenneth-phase38-host > /results/phase38-host-symbols.txt
for ndk in AAssetManager_fromJava AAssetManager_open AAsset_read \
           AAsset_seek64 AAsset_getLength64 AAsset_close; do
  grep -Eq " U $ndk$" /results/phase38-guest-symbols.txt || {
    echo "ERROR: missing guest import $ndk"; exit 48
  }
  grep -Eq " T $ndk$" /results/phase38-host-symbols.txt || {
    echo "ERROR: missing host export $ndk"; exit 48
  }
done
timeout 25 /usr/bin/time -f 'peak_rss_kb=%M elapsed=%e' \
  /results/lenneth-phase38-host > /results/phase38.log 2>&1 || {
    cat /results/phase38.log
    printf 'phase38\tFAIL\tunknown\n' >> suite.tsv
    exit 48
  }
grep -q 'PHASE38_ASSET_BRIDGE=PASS' /results/phase38.log
grep -q 'traversal_symlink=DENIED' /results/phase38.log
grep -q 'refs_remaining=0' /results/phase38.log
grep -q 'STREAMED_ASSET_BOOT[[:space:]]*PASS' /results/lenneth-asset-bridge38.tsv
grep -q 'TRAVERSAL_AND_SYMLINK_DENIAL[[:space:]]*PASS' /results/lenneth-asset-bridge38.tsv
grep -q 'ASSET_AND_GLOBAL_REF_LEAKS[[:space:]]*PASS' /results/lenneth-asset-bridge38.tsv
grep -q 'ACTUAL_APK_ASSET_MANAGER[[:space:]]*BLOCKED' /results/lenneth-asset-bridge38.tsv
rss38=$(grep -o 'peak_rss_kb=[0-9]*' /results/phase38.log | tail -n1 | cut -d= -f2 || true)
if [ -z "$rss38" ]; then rss38=unknown; fi
printf 'phase38\tPASS\t%s\n' "$rss38" >> suite.tsv
rm -f /results/lenneth-phase38-host /results/lenneth-phase38-fixture.so
# Phase39: read-only dynamic import/DT_NEEDED audit of mock ARM64 .so files.
# No real APK, commercial libraries, or game code in public CI.
cc -fPIC -shared -nostdlib -Wl,-soname,libphase39provider.so \
  -o /results/libphase39provider.so /work/lenneth-r36s/cloud/phase39_provider.c
cc -fPIC -shared -nostdlib -Wl,-soname,libphase39consumer.so \
  -Wl,--no-as-needed \
  -o /results/libphase39consumer.so /work/lenneth-r36s/cloud/phase39_consumer.c \
  -L /results -lphase39provider
readelf -h /results/libphase39consumer.so | grep -q 'Machine:.*AArch64'
readelf -d /results/libphase39consumer.so | grep -q 'libphase39provider.so'
python3 -B /work/lenneth-r36s/cloud/elf_dependencies39.py \
  --modules /results/libphase39consumer.so /results/libphase39provider.so \
  --output /results/lenneth-elf-dependencies39.tsv --fixture-check \
  > /results/phase39.log || { cat /results/phase39.log; exit 49; }
grep -q 'PHASE39_ELF_DEPENDENCIES=PASS' /results/phase39.log
grep -q 'cross_elf=PASS missing_provider=REPORTED' /results/phase39.log
grep -q $'IMPORT\tCROSS_ELF_CANDIDATE\tphase39_shared_api' /results/lenneth-elf-dependencies39.tsv
grep -q $'IMPORT\tPROVIDER_NOT_ESTABLISHED\tphase39_missing_api' /results/lenneth-elf-dependencies39.tsv
grep -q $'IMPORT\tWEAK_OPTIONAL\tphase39_weak_api' /results/lenneth-elf-dependencies39.tsv
grep -q $'RELOCATION_AND_SYMBOL_VERSIONS\tUNVERIFIED' /results/lenneth-elf-dependencies39.tsv
grep -q $'COMMERCIAL_ENGINE\tNOT_EXECUTED' /results/lenneth-elf-dependencies39.tsv
printf 'phase39\tPASS\tELF_metadata_only\n' >> suite.tsv
rm -f /results/libphase39provider.so /results/libphase39consumer.so
# Phase40: fail-closed read-only startup blocker triage using synthetic TSV.
# The user's actual diagnostic ZIP is NOT committed or uploaded to CI.
python3 -B /work/lenneth-r36s/cloud/startup_gate40.py \
  --selftest \
  --output /results/lenneth-startup-priorities40.tsv \
  --summary /results/lenneth-startup-summary40.txt \
  > /results/phase40.log || {
    cat /results/phase40.log
    printf 'phase40\tFAIL\tmetadata_only\n' >> suite.tsv
    exit 50
  }
grep -q 'PHASE40_STARTUP_TRIAGE=PASS' /results/phase40.log
grep -q 'incomplete_relocations=DENIED' /results/phase40.log
grep -q 'missing_jni_gate=DENIED' /results/phase40.log
grep -q '^GAME_BOOT_READY=NO$' /results/lenneth-startup-summary40.txt
grep -q '^COMMERCIAL_ENGINE_EXECUTED=NO$' /results/lenneth-startup-summary40.txt
grep -q '^SOURCE=SYNTHETIC_FIXTURE$' /results/lenneth-startup-summary40.txt
grep -q 'BIONIC_CPP_CONSTRUCTORS' /results/lenneth-startup-priorities40.tsv
grep -q 'JNI_ART_CLASSLOADER' /results/lenneth-startup-priorities40.tsv
printf 'phase40\tPASS\tstartup_metadata_only\n' >> suite.tsv
count=$(awk -F '\t' '$2=="PASS" {n++} END {print n+0}' suite.tsv)
test "$count" -eq 36
printf 'CLOUD_DEBIAN13_FINAL=PASS selftests=%s phase34=PASS phase35=PASS phase36=PASS phase37=PASS phase38=PASS phase39=PASS phase40=PASS commercial_engine=NO\n' "$count" | tee result.txt
