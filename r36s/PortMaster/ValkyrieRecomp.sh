#!/bin/sh
# Experimental manual Ports shortcut for R36S / DarkOSRE.
# Always write an early diagnostic log, even if the game executable is absent.
set -u

ports_dir=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 1
game_dir="$ports_dir/valkyrierecomp"
launcher="$game_dir/ValkyrieRecomp.sh"
mkdir -p "$game_dir" 2>/dev/null || :
log="$game_dir/valkyrie-r36s.log"
if ! ( : >> "$log" ) 2>/dev/null; then
    log="/tmp/valkyrie-r36s.log"
fi
exec >> "$log" 2>&1

printf '\n=== ValkyrieRecomp Ports start: %s ===\n' "$(date 2>/dev/null || echo sem-data)"
printf '[ports] Script: %s\n' "$0"
printf '[ports] Game directory: %s\n' "$game_dir"
printf '[ports] Architecture: %s\n' "$(uname -m 2>/dev/null || echo desconhecida)"
printf '[ports] Log: %s\n' "$log"

if [ ! -f "$launcher" ]; then
    printf '[ERRO] Launcher interno ausente: %s\n' "$launcher"
    exit 1
fi

# /bin/sh avoids requiring Bash for the early bring-up and diagnostics.
/bin/sh "$launcher" "$@"
rc=$?
printf '[ports] Launcher returned status: %s\n' "$rc"
exit "$rc"
