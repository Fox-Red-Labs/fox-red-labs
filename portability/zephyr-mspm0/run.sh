#!/bin/bash
# Flash an image into the LP-MSPM0G3519 (probe serial PROBE, required; console PORT, default /dev/cu.usbmodem<PROBE>1) and capture its console (115200, backchannel UART).
#   run.sh <image.hex> <log> [seconds=14]
# The capture holds the port open (am62l-evm/tools/capture.py), never stty then cat.
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
[ $# -ge 2 ] || { echo "usage: run.sh <image.hex> <log> [seconds=14]" >&2; exit 2; }
HEX="$1"; LOG="$2"; SECS="${3:-14}"; PROBE="${PROBE:?set PROBE to the probe serial}"; PORT="${PORT:-/dev/cu.usbmodem${PROBE}1}"
need PORT "$PORT"; need hex "$HEX"
rm -f "$LOG"
python3 "$REPO/am62l-evm/tools/capture.py" $PORT 115200 "$LOG" >/dev/null 2>&1 & CP=$!
sleep 1
"$HERE/flash.sh" "$HEX" > "${LOG%.txt}.flash.txt" 2>&1 || { kill $CP; echo "flash failed, see ${LOG%.txt}.flash.txt"; exit 1; }
sleep "$SECS"; kill $CP 2>/dev/null || true
