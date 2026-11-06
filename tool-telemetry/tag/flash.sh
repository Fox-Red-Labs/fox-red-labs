#!/usr/bin/env bash
# Flash the tool tag built by build.sh, then print 12 s of its serial output.
#   tool-telemetry/tag/flash.sh [cc2340r5]
# Uses the board folder's connection file (probe picked by serial number). DSLite's default load erases the chip first,
# so save what is on the board before the first flash (tool-telemetry/README.md, "Before you flash").
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
BOARD="${1:-cc2340r5}"
case "$BOARD" in
	cc2340r5) CCXML="$REPO/cc2340r5/CC2340R5.ccxml" ;;
	cc2755p10) CCXML="${FRL_CC2755_CCXML:-$PWD/out/tool-telemetry/hubble/cc2755p10-LB07021W.ccxml}" ;;   # its LP-XDS110's serial
	*) echo "board must be cc2340r5 or cc2755p10"; exit 2 ;;
esac
PROBE=$(grep -o '\(LS\|LB\)[0-9A-Z]\{6\}' "$CCXML" | head -1)
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
OUT="$PWD/out/tool-telemetry/tag-$BOARD"
"$DSLITE" flash -c "$CCXML" -e -f -u "$OUT/tooltag.hex" > "$OUT/flash.log" 2>&1 || { tail -5 "$OUT/flash.log"; exit 1; }
echo "flashed $OUT/tooltag.hex to probe $PROBE"
PY="${FRL_BLE_PY:-$HOME/.venvs/ble/bin/python}"
"$PY" "$HERE/../sim/serialmon.py" "/dev/cu.usbmodem${PROBE}1" 12 --reset "$PROBE"
