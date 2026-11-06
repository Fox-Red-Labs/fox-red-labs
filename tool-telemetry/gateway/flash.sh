#!/usr/bin/env bash
# Program the gateway image that build.sh signed into the LP-EM-CC35X1 (probe from PROBE by default), then print 25 s
# of its serial output. Replaces whatever program is on the board; the chip's activation is not touched.
#   tool-telemetry/gateway/flash.sh [probe serial]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/tool-telemetry/gateway"
PROBE="${1:-${PROBE:?set PROBE or pass the probe serial as the first argument}}"; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
TS="$RUN/nt/freertos/ticlang/toolbox/tool_settings.json"; [ -f "$TS" ] || { echo "build first: $TS missing"; exit 1; }
( cd "$TB" && ./simplelink-wifi-toolbox programmer -i XDS110 -param1 "$PROBE" programming --tool_settings "$TS" \
	--report_file_name_path "$RUN/program-report.bin" ) > "$RUN/program.log" 2>&1 || { tail -8 "$RUN/program.log"; exit 1; }
echo "programmed ($RUN/program.log)"
PY="${FRL_BLE_PY:-$HOME/.venvs/ble/bin/python}"
# No probe reset here: the programmer already starts the new image, and an XDS110 reset after it leaves the CC35X1
# silent until a power cycle (seen 2026-10-17). Open the port first so the boot lines are caught.
"$PY" "$HERE/../sim/serialmon.py" "/dev/cu.usbmodem${PROBE}1" "${SECS:-25}"
