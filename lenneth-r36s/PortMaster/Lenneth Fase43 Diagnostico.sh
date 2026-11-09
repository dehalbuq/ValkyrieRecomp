#!/bin/sh
# Private read-only diagnostic entrypoint for R36S Ports.
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec /bin/bash "$root/lenneth-fase42/phase43-preflight.sh"
