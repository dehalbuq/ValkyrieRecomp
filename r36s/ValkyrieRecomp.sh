#!/bin/sh
# Experimental R36S launcher. A compatible compiled ARM64 game binary is required.
# Log initialization and validation failures before trying to start the game.
set -u

here=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 1
cd "$here" || exit 1
log="$here/valkyrie-r36s.log"
if ! ( : >> "$log" ) 2>/dev/null; then
    log="/tmp/valkyrie-r36s.log"
fi
exec >> "$log" 2>&1

printf '[launcher] Start: %s\n' "$(date 2>/dev/null || echo sem-data)"
printf '[launcher] Working directory: %s\n' "$here"
printf '[launcher] Architecture: %s\n' "$(uname -m 2>/dev/null || echo desconhecida)"

aspect=${VALKYRIE_ASPECT:-4:3}
case "$aspect" in
    4:3|16:9) ;;
    *) printf '[ERRO] VALKYRIE_ASPECT deve ser 4:3 ou 16:9; atual: %s\n' "$aspect"; exit 2 ;;
esac

disc_num=${VALKYRIE_DISC:-1}
case "$disc_num" in
    1|2) ;;
    *) printf '[ERRO] VALKYRIE_DISC deve ser 1 ou 2; atual: %s\n' "$disc_num"; exit 2 ;;
esac

disc="disc/Disc${disc_num}/Valkyrie Profile (USA) (Disc ${disc_num}).cue"
bin=${VALKYRIE_BIN:-./ValkyrieRecomp}
printf '[launcher] Disc: %s | Aspect: %s | Renderer: software\n' "$disc_num" "$aspect"
printf '[launcher] Executable: %s\n' "$bin"

if [ ! -f "$bin" ]; then
    printf '[ERRO] O executavel ARM64 ainda nao existe em %s.\n' "$bin"
    printf '[ACAO] O pacote de preparacao contem pastas/scripts, mas NAO contem o jogo compilado.\n'
    exit 127
fi
if [ ! -x "$bin" ]; then
    printf '[ERRO] O arquivo existe mas nao tem permissao de execucao: %s\n' "$bin"
    printf '[ACAO] Verifique permissoes e arquitetura; exemplo: chmod +x "%s"\n' "$bin"
    exit 126
fi
if command -v file >/dev/null 2>&1; then
    file "$bin" || :
fi
if [ ! -f game.toml ]; then
    printf '[ERRO] game.toml nao encontrado em %s\n' "$here"
    exit 1
fi
if [ ! -f "$disc" ]; then
    printf '[ERRO] Imagem do disco ausente: %s\n' "$disc"
    exit 1
fi

# Keep the desktop original unchanged and write handheld settings separately.
if ! awk -v aspect="$aspect" '
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
' game.toml > game-r36s.local.toml; then
    printf '[ERRO] Falha ao gerar game-r36s.local.toml.\n'
    exit 1
fi

mkdir -p saves || exit 1
LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
PSX_NO_LAUNCHER=1
export LD_LIBRARY_PATH PSX_NO_LAUNCHER
printf '[launcher] Iniciando binario; todas as mensagens serao registradas neste log.\n'
exec "$bin" --game game-r36s.local.toml --disc "$disc" \
    --no-launcher --renderer software
