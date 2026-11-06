#!/bin/bash
# Program one signed build onto the LP-EM-CC35X1 (probe in PROBE) while holding the probe lock, reset it, and capture the console.
#   ./flash_run.sh <build folder> <console log> [seconds=60] [until regex]
# <build folder> is what build.sh printed up to /ex (e.g. out/dns/build-min). Run it through the lock:
#   python3 tools/frl/lock.py exec --serial <probe> -- ports/dns-sinkhole/flash_run.sh <build folder> <log>
set -u
B=$1; OUT=$2; W=${3:-60}; UNTIL=${4:-NEVERMATCH}
HERE="$(cd "$(dirname "$0")" && pwd)"; CAP="$HERE/../../cc35x1/bench/cap.py"
PY=${PY:-$HOME/.venvs/csi/bin/python}; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
RST=/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset; PROBE="${PROBE:?set PROBE to the probe serial}"
TS="$B/ex/freertos/ticlang/toolbox/tool_settings.json"; [ -f "$TS" ] || { echo "no $TS"; exit 1; }
ok=0
for try in 1 2 3 4 5 6; do
	( cd "$TB" && ./simplelink-wifi-toolbox programmer -i XDS110 -param1 $PROBE programming --tool_settings "$TS" ) > "$OUT.program.log" 2>&1 && { ok=1; break; }
	echo "programming try $try failed ($(grep -o 'error code -[0-9]*' "$OUT.program.log" | head -1)); retry in 30 s"; sleep 30
done
[ $ok = 1 ] || { echo "programming FAILED, see $OUT.program.log"; exit 2; }
echo "programmed $(date +%T)"
"$PY" "$CAP" "$OUT" "$W" --until "$UNTIL" & CP=$!
sleep 1; "$RST" -s $PROBE > /dev/null 2>&1
wait $CP; echo "capture rc=$?"
