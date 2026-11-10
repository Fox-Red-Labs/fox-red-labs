#!/bin/bash
# Read-only: list the debug cores of an AM243x LaunchPad. am243x/tools/list_cores.sh <XDS110 serial> [device xml name]
set -u
S=${1:?serial}; DEV=${2:-AM243x_LP}
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
C="$REPO/am243x/${DEV}_$S.ccxml"
O="$PWD/out/am243x/identify"; mkdir -p "$O"
[ -f "$C" ] || { echo "no $C (committed connection files: am243x/*.ccxml)"; exit 1; }
/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash --config="$C" --list-cores 2>&1 | tee "$O/list-cores.log"
