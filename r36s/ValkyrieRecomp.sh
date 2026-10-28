#!/usr/bin/env bash
# Copy this script into the same directory as ValkyrieRecomp and game.toml.
# Does NOT contain game data. An R36S-compatible ARM64 binary is required.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

aspect="${VALKYRIE_ASPECT:-4:3}"
case "$aspect" in
    "4:3"|"16:9") ;;
    *) echo "VALKYRIE_ASPECT must be 4:3 or 16:9" >&2; exit 2 ;;
esac

disc_num="${VALKYRIE_DISC:-1}"
case "$disc_num" in
    1|2) ;;
    *) echo "VALKYRIE_DISC must be 1 or 2" >&2; exit 2 ;;
esac
disc="disc/Disc${disc_num}/Valkyrie Profile (USA) (Disc ${disc_num}).cue"
bin="${VALKYRIE_BIN:-./ValkyrieRecomp}"

if [[ ! -x "$bin" ]]; then
    echo "ARM64 executable not found or not executable: $bin" >&2
    exit 1
fi
if [[ ! -f game.toml || ! -f "$disc" ]]; then
    echo "Missing game.toml or legally obtained game disc: $disc" >&2
    exit 1
fi

# Preserve the original configuration (and its 16:9 mode on PC).
# Keep this generated config BESIDE game.toml so relative disc/save paths work.
awk -v aspect="$aspect" '
    /^\[video\][[:space:]]*$/ {
        in_video = 1
        seen_video = 1
        print
        print "renderer = \"software\""
        print "aspect_ratio = \"" aspect "\""
        print "supersampling = 1"
        print "window_width = 640"
        next
    }
    /^\[/ { in_video = 0 }
    in_video && /^[[:space:]]*(renderer|aspect_ratio|supersampling|window_width)[[:space:]]*=/ { next }
    { print }
    END { if (!seen_video) exit 2 }
' game.toml > game-r36s.local.toml

mkdir -p saves
# Keep target-ABI libraries alongside the program if needed.
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PSX_NO_LAUNCHER=1
log="$PWD/valkyrie-r36s.log"
printf '\n[%s] Starting ValkyrieRecomp disc=%s aspect=%s renderer=software\n' \
    "$(date)" "$disc_num" "$aspect" >> "$log"
exec "$bin" --game game-r36s.local.toml --disc "$disc" \
    --no-launcher --renderer software >> "$log" 2>&1
