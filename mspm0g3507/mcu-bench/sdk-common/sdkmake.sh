#!/bin/bash
# Build a copied MSPM0 SDK example from its ticlang/ folder with the tools on this computer.
# Usage: sdkmake.sh <example>/ticlang [extra make args]
# The SDK, SysConfig and TI Arm Clang are found the way tools/frl finds them (TI's default install folders,
# or FRL_MSPM0_SDK, FRL_SYSCONFIG and FRL_TICLANG to choose); the validated versions win, and an ambiguous or
# unvalidated set stops with the choices rather than taking the first folder found.
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TOOLS=()
while IFS= read -r -d '' a; do TOOLS+=("$a"); done < <(python3 "$ROOT/tools/frl/toolenv.py" mspm0)
[ ${#TOOLS[@]} -eq 4 ] || exit 2
cd "$1"; shift
make "${TOOLS[@]}" "$@"
