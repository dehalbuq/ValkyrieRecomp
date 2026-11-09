#!/bin/bash
# Phase43: private read-only ELF batch for R36S; no guest code execution.
set -u
umask 077
ulimit -c 0 2>/dev/null || true
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)" || exit 2
cd "$root" || exit 2
printf 'PHASE43_STARTED\n' > "$root/Lenneth_Fase43_INICIO.log" 2>/dev/null || true
mkdir -p "$root/diagnostico-fase43"
session="$(mktemp -d "$root/diagnostico-fase43/sessao.XXXXXX")" || exit 2
printf 'teste\tstatus\tcodigo\n' > "$session/resultados43.tsv"
printf 'commercial_engine=NOT_EXECUTED\nRMS=NOT_EXECUTED\n' > "$session/resumo43.txt"
printf 'arch=%s\nkernel=%s\n' "$(uname -m)" "$(uname -r)" > "$session/ambiente43.txt"
finish() {
  printf 'status=%s\ncommercial_engine=NOT_EXECUTED\nRMS=NOT_EXECUTED\n' "$1" > "$session/resumo43.txt"
  tar -czf "$root/Lenneth_Fase43_DIAGNOSTICOS.tar.gz" -C "$root/diagnostico-fase43" "$(basename "$session")" 2>> "$session/execucao43.log" || true
  printf 'PHASE43_END status=%s\n' "$1" >> "$root/Lenneth_Fase43_INICIO.log" 2>/dev/null || true
}
probe="$root/lenneth-probe"
if [ ! -f "$probe" ]; then
  printf 'PROBE_AUSENTE\n' > "$session/execucao43.log"
  finish SEM_PROBE
  exit 0
fi
chmod +x "$probe" 2>/dev/null || true
game="$root/libMyGame.so"
cxx="$root/libc++_shared.so"
if [ ! -f "$game" ]; then game="$root/lib/arm64-v8a/libMyGame.so"; fi
if [ ! -f "$cxx" ]; then cxx="$root/lib/arm64-v8a/libc++_shared.so"; fi
if [ ! -f "$game" ] || [ ! -f "$cxx" ]; then
  printf 'BIBLIOTECAS_AUSENTES\n' > "$session/execucao43.log"
  finish SEM_BIBLIOTECAS
  exit 0
fi
if [ "$(uname -m)" != aarch64 ]; then
  printf 'ARCH_INCORRETA\n' > "$session/execucao43.log"
  finish SEM_ARM64
  exit 0
fi
if ! command -v timeout >/dev/null 2>&1; then
  printf 'TIMEOUT_AUSENTE\n' > "$session/execucao43.log"
  finish SEM_TIMEOUT
  exit 0
fi
for f in "$game" "$cxx"; do sha256sum "$f" >> "$session/hashes43.txt" 2>/dev/null || true; done
mkdir -p assets saves
for f in lenneth-suite-summary.txt lenneth-blockers.tsv lenneth-startup-gates.tsv lenneth-phase25-startup-plan.tsv lenneth-jni-demand31.tsv lenneth-jni-contract32.tsv lenneth-jni-services33.tsv lenneth-gles1-runtime26.tsv; do rm -f -- "$f" 2>/dev/null || true; done
printf 'BATCH_HEADLESS_STARTED guest_code=NOT_EXECUTED\n' > "$session/execucao43.log"
timeout -k 5s 110s "$probe" --batch-headless "$game" "$cxx" > "$session/batch43.log" 2>&1
rc=$?
printf 'batch_headless\t%s\t%s\n' "$([ "$rc" -eq 0 ] && echo PASS || echo FALHA)" "$rc" >> "$session/resultados43.tsv"
for f in lenneth-suite-summary.txt lenneth-blockers.tsv lenneth-startup-gates.tsv lenneth-phase25-startup-plan.tsv lenneth-jni-demand31.tsv lenneth-jni-contract32.tsv lenneth-jni-services33.tsv lenneth-gles1-runtime26.tsv; do
  if [ -f "$f" ]; then cp -f "$f" "$session/$f" 2>/dev/null || true; fi
done
printf 'BATCH_HEADLESS_EXIT=%s\n' "$rc" >> "$session/execucao43.log"
if [ "$rc" -eq 0 ]; then finish FINALIZADO; else finish FALHA_BATERIA; fi
