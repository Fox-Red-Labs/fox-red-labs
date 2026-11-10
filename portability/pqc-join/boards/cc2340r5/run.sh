#!/bin/bash
# Flash one pqc image to the LP-EM-CC2340R5 and capture its report, N times (separate loads).
#   run under the probe lock:  python3 tools/frl/lock.py exec --serial LS470GTU -- ./run.sh <image dir> <run-log prefix> [runs=3]
# <image dir> is a build.sh output folder (image.hex, build-id.txt, ccfg.txt). Does not restore the board; do that afterwards with the backup.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
D="${1:?image dir}"; P="${2:?log prefix}"; N="${3:-3}"
DSLITE=/Applications/ti/uniflash_9.6.0/dslite.sh; CCXML="$REPO/cc2340r5/CC2340R5.ccxml"; DEV="${SERIAL_DEV:-/dev/cu.usbmodemLS470GTU1}"
grep -q CCFG_PERMISSION_ALLOW "$D/ccfg.txt" || { echo "no CCFG check"; exit 4; }
for k in $(seq 1 "$N"); do
  python3 "$REPO/cc2340r5/bench/lib/capture.py" "$DEV" "$(cat "$D/build-id.txt")" "$P.run$k.txt" 240 &
  L=$!; sleep 1
  "$DSLITE" --mode flash -c "$CCXML" -e -f -u "$D/image.hex" > "$P.run$k.flash.txt" 2>&1
  tail -1 "$P.run$k.flash.txt"
  wait $L; echo "run $k capture exit $?"
done
