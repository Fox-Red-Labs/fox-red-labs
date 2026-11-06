#!/bin/bash
# Read-only: list the debug cores of an AM263Px board. am263p/tools/list_cores.sh <XDS110 serial> [device xml name]
set -u
S=${1:?serial}; DEV=${2:-AM263Px_ZCZ_C}
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
C="$REPO/am263p/${DEV}_$S.ccxml"
O="$PWD/out/am263p/identify"; mkdir -p "$O"
sed -e "s/@SERIAL@/$S/g" -e "s/@DEV@/$DEV/g" "$(dirname "$0")/ccxml.template" > "$C"
/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash --config="$C" --list-cores 2>&1 | tee "$O/list-cores.log"
