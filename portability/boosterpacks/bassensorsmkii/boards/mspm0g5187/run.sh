#!/bin/bash
# Build, flash and read the LP-MSPM0G5187 (probe MG020001) through the probe lock.
#   boards/mspm0g5187/run.sh hello 20 results/hello.txt          one line a second for 20 s
#   boards/mspm0g5187/run.sh motion_stream 62 results/raw.txt    100 Hz CSV for one minute, then:
#   python3 tools/analyze_motion.py results/raw.txt results/motion.csv
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; S="$HERE/../.."; ROOT="$S/../../.."
APP="${1:-hello}"; SECS="${2:-20}"; CAP="${3:-$S/results/$APP-$(date +%H%M%S).txt}"
"$HERE/../build.sh" mspm0g5187 "$APP"
PY="${PY:-$HOME/zephyrproject/.venv/bin/python3}"   # any Python with pyserial
"$PY" "$ROOT/tools/frl/lock.py" exec --serial "${PROBE:?set PROBE to the probe serial}" -- "$PY" "$S/tools/flash_capture.py" \
  "${BUILD_OUT:-$PWD/out/boosterpack-speedrun/bassensors}/mspm0g5187/$APP/$APP.out" "$SECS" "$CAP"
