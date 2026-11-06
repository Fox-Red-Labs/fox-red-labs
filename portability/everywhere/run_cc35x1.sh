#!/bin/bash
# Run built Wi-Fi SDK projects on the LP-EM-CC35X1 (probe in PROBE) one at a time through the SimpleLink Wi-Fi Toolbox:
# sign the .out into a flash image, program it, reset, read the serial port, record one JSON line. Modeled on
# cc35x1/sweep/run_pass.sh. The board cannot be backed up by the debugger: the caller restores a known image after.
#   run_cc35x1.sh <results jsonl> <seconds> <ticlang build dir> [<ticlang build dir> ...]
set -u
OUT="$1"; SECS="$2"; shift 2
PROBE="${PROBE:?set PROBE to the probe serial}"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
X=/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset
SER="$(cd "$(dirname "$0")/../.." && pwd)/cc35x1/sweep/ser.py"; PORT=${PORT:-/dev/cu.usbmodem${PROBE}1}
for D in "$@"; do
  e=$(basename "$(cd "$D/../.." && pwd)"); art=$(ls "$D"/*.out 2>/dev/null | head -1); p=skip; bytes=0; first=""; flags=""
  [ -n "$art" ] || { echo "{\"example\":\"$e\",\"programmed\":false,\"why\":\"no .out\"}" >> "$OUT"; continue; }
  make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$D" BUILD_DIR="$D" BUILD_ARTIFACT="$art" TOOLBOX_DIR="$TB" > "$D/img.log" 2>&1 || p=IMGFAIL
  if [ $p = skip ]; then
    J="$D/toolbox/tool_settings.json"; sed -i '' "s#\"freertos/ticlang#\"$D#" "$J"
    (cd "$TB"; ./simplelink-wifi-toolbox programmer -i XDS110 -param1 "$PROBE" programming --tool_settings "$J" --report_file_name_path "$D/r.bin" > "$D/p.log" 2>&1); [ -s "$D/r.bin" ] && p=ok || p=FAIL
  fi
  if [ $p = ok ]; then
    ( sleep 1; $X -s "$PROBE" >/dev/null 2>&1 ) & python3 "$SER" $PORT 115200 "$SECS" > "$D/s.txt" 2>/dev/null; wait
    bytes=$(wc -c < "$D/s.txt" | tr -d ' '); first=$(strings "$D/s.txt" | grep -v "^$" | head -1 | tr '"' "'" | cut -c1-80)
    strings "$D/s.txt" | grep -qiE "exception|fault|assert|panic" && flags="fault-words"
  fi
  echo "{\"example\":\"$e\",\"programmed\":$([ $p = ok ] && echo true || echo false),\"step\":\"$p\",\"serial_bytes\":$bytes,\"flags\":\"$flags\",\"first_line\":\"$first\",\"date\":\"$(date +%F)\"}" >> "$OUT"
  echo "$e $p bytes=$bytes $flags $first"
done
