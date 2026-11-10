#!/bin/sh
# Start from the R36S/PortMaster Ports menu.
set -u
BASE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)" || exit 2
DIR="$BASE/vplnx-r36s"
if [ ! -d "$DIR" ]; then mkdir -p "$DIR" 2>/dev/null || exit 2; fi
if [ ! -f "$DIR/run-r36s-diagnostico.sh" ]; then
    printf '%s\n' "ERRO: run-r36s-diagnostico.sh ausente; reinstale o pacote completo." \
      > "$DIR/VPLNX_R36S_INICIO.log"
    exit 2
fi
exec /bin/sh "$DIR/run-r36s-diagnostico.sh"
