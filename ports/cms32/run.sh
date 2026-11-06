#!/bin/bash
# Flash one built example to the LP-MSPM0L1306 (probe ML130001) and record what it does.
#   ./run.sh <example> [seconds] [baud]      e.g. ./run.sh uart/uartSendReceive 8 115200
# Flashes with TI's DSLite (OpenOCD could not reach this chip on 2026-10-07; DSLite could), captures the
# board's USB serial port for <seconds> into runs/<name>.txt, then reads GPIOA's output register three times
# a second apart with OpenOCD if it can attach (a changing bit is a blinking LED). Nothing else is touched.
set -eo pipefail
EX="$1"; S=${2:-6}; BAUD=${3:-19200}
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../.."
NAME="${EX//\//_}"; OUT="$HERE/build/$NAME/$NAME.out"; [ -f "$OUT" ] || { echo "build first: $OUT"; exit 2; }
mkdir -p "$HERE/runs"; LOG="$HERE/runs/$NAME.txt"
DSLITE=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
CCXML="$R/mspm0l1306/MSPM0L1306_ML130001.ccxml"
TTY=/dev/cu.usbmodemML1300011
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e 2>/dev/null | grep -q "Serial Num: *${PROBE:?set PROBE to the probe serial}" || { echo "Probe $PROBE missing"; exit 3; }
{
  echo "# $EX  $(date '+%Y-%m-%d %H:%M:%S')  image $OUT  baud $BAUD  capture ${S}s"
  # open the serial port first so the first lines after reset are caught
  python3 "$HERE/capture.py" "$TTY" "$BAUD" $((S + 8)) "$HERE/runs/$NAME.serial" &
  CAT=$!
  sleep 0.5
  t0=$(date +%s.%N)
  # Keep DSLite's own exit status: a failed load must stop here, not capture the firmware that was already on the board.
  if ! DOUT=$("$DSLITE" load --config="$CCXML" -f "$OUT" 2>&1); then
    printf '%s\n' "$DOUT" | grep -vE "^\s*$" | sed 's/^/dslite: /' || true
    kill $CAT 2>/dev/null || true; wait $CAT 2>/dev/null || true; rm -f "$HERE/runs/$NAME.serial"
    echo "# FLASH FAILED: DSLite did not load $OUT. Nothing captured (the board may still run older firmware)."
    exit 5
  fi
  printf '%s\n' "$DOUT" | grep -vE "^\s*$" | sed 's/^/dslite: /' || true
  echo "# flashed in $(printf '%.1f' "$(echo "$(date +%s.%N) - $t0" | bc)") s, board running"
  sleep "$S"
  CAPRC=0
  if kill -0 $CAT 2>/dev/null; then kill $CAT 2>/dev/null || true; wait $CAT 2>/dev/null || true; else wait $CAT || CAPRC=$?; fi
  echo "# serial (${S}s):"
  { tr -d '\0' < "$HERE/runs/$NAME.serial" | head -c 4000 | sed 's/^/  | /'; } || true
  echo
  echo "# GPIOA DOUT31_0 (0x400A1280), five reads at uneven gaps (0, 330, 770, 1130, 1600 ms) via OpenOCD; a toggling pin shows different values:"
  "$R/mspm0l1306/tools/ocd.sh" 'echo "DOUT [mdw 0x400A1280 1]"' "sleep 330" 'echo "DOUT [mdw 0x400A1280 1]"' "sleep 440" 'echo "DOUT [mdw 0x400A1280 1]"' "sleep 360" 'echo "DOUT [mdw 0x400A1280 1]"' "sleep 470" 'echo "DOUT [mdw 0x400A1280 1]"' 2>&1 | grep -E "^DOUT|Could not find" | head -6 || true
  [ "$CAPRC" -eq 0 ] || { echo "# SERIAL CAPTURE FAILED (capture.py exit $CAPRC): the serial section above is not a record of this run."; exit 4; }
} | tee "$LOG"
