#!/bin/bash
# Run the built image on the board and check its serial output. Hold the probe lock around this:
#   python3 tools/frl/lock.py exec --serial CL371081 -- portability/three-ways/c2000/run_board.sh <port> <log dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../../.."
PORT=${1:?serial port}; LOGD=${2:?log dir}; mkdir -p "$LOGD"
/Applications/ti/ccs2101/ccs/scripting/run.sh "$HERE/run_board.js" "$HERE/build-f2800137/three_ways_c2000.out" "$R/c2000-launchpad/tools/F2800137_CL371081.ccxml" 30 > "$LOGD/run.txt" 2>&1 &
for i in $(seq 1 60); do grep -q RUNNING "$LOGD/run.txt" 2>/dev/null && break; sleep 0.5; done
python3 "$HERE/../serial_check.py" "$PORT" 14 "$LOGD/serial.txt"
wait
