#!/bin/sh
# PortMaster menu entry: native ARM64 compatibility test, not playable game.
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec /bin/bash "$root/lenneth-r36s/launch.sh"
