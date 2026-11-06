#!/bin/bash
# Program one signed build onto the LP-EM-CC35X1 (probe in PROBE), reset it. Run through the probe lock:
#   python3 tools/frl/lock.py exec --serial <probe> -- ports/doom/target-cc35x1/flash.sh <tool_settings.json> <log>
set -u
TS=$1; OUT=$2; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23; PROBE="${PROBE:?set PROBE to the probe serial}"
RST=/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset
[ -f "$TS" ] || { echo "no $TS"; exit 1; }
for try in 1 2 3 4; do
	T0=$(date +%s)
	( cd "$TB" && ./simplelink-wifi-toolbox programmer -i XDS110 -param1 $PROBE programming --tool_settings "$TS" ) > "$OUT" 2>&1 && { echo "programmed in $(( $(date +%s) - T0 )) s"; "$RST" -s $PROBE > /dev/null 2>&1; exit 0; }
	echo "try $try failed ($(grep -o 'error code -[0-9]*' "$OUT" | head -1)); retry in 20 s"; sleep 20
done
echo "programming FAILED, see $OUT"; exit 2
