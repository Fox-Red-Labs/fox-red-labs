#!/bin/bash
# Run the post-quantum join on the LP-CC1354P10-1 (probe LS45013A) and put the board back as it was.
#   UHUB=20-4:1 RESTORE_IMG=<.out of the image to leave running> ./run.sh [-O3]
# Needs: the board on a USB port the Mac can switch (uhubctl), nobody else using the probe, ./build.sh already run.
# The board's debugger connects only just after a power-on boot, so the script power-cycles the port, then in one CCStudio
# session backs up flash and CCFG, loads and starts the image (ccs_pqc_session.py) while mac_side.py talks to it on the
# backchannel UART. Everything lands in $RUN (default out/pqc-join-<date>, outside the repo).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
OPT="${1:--O3}"; IMG="$HERE/out/pqc-join${OPT}${LOWRAM:+-lowram}/image.out"
: "${UHUB:?set UHUB=<hub>:<port>, e.g. 20-4:1}"; : "${RESTORE_IMG:?set RESTORE_IMG=<.out to leave running>}"
RUN="${RUN:-$PWD/out/pqc-join-$(date +%Y%m%d-%H%M%S)}"; mkdir -p "$RUN"; rm -f "$RUN/running" "$RUN/stop"
DEV=/dev/cu.usbmodemLS45013A1; CCSPY=/Applications/ti/ccs2101/ccs/scripting/python/run.sh
[ -f "$IMG" ] && [ -f "$RESTORE_IMG" ] || { echo "missing image"; exit 4; }
for i in $(seq 30); do pgrep -P 1 -x DSLite > /dev/null || break; sleep 1; done
uhubctl -l "${UHUB%%:*}" -p "${UHUB##*:}" -a off > /dev/null 2>&1; sleep 6
uhubctl -l "${UHUB%%:*}" -p "${UHUB##*:}" -a on > /dev/null 2>&1
for i in $(seq 30); do [ -e "$DEV" ] && break; sleep 1; done; sleep 2
"$CCSPY" "$HERE/ccs_pqc_session.py" "$REPO/cc1354p10/CC1354P10.ccxml" "$IMG" "$RESTORE_IMG" "$RUN" 600 > "$RUN/session.out" 2>&1 &
SP=$!
[ -n "${PEEK:-}" ] && ( sleep "$PEEK"; touch "$RUN/peek" ) &
python3 -u -I "$REPO/tools/pqc_join/mac_side.py" --serial "$DEV" --timeout 240 --json "$RUN/join.json" > "$RUN/mac_side.txt" 2>&1 &
MP=$!
wait $MP; MRC=$?; echo "mac_side exit $MRC"
# the board repeats its whole report every 10 s after the join: capture one full repeat
python3 -I - "$DEV" "$RUN/report.txt" <<'PY'
import os, select, sys, termios, time
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[0] = a[1] = a[3] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)
buf, end = b"", time.time() + 26
while time.time() < end and buf.count(b"BENCH DONE") < 1:
    if select.select([fd], [], [], 0.3)[0]: buf += os.read(fd, 4096)
open(sys.argv[2], "wb").write(buf)
PY
touch "$RUN/stop"; wait $SP; SRC=$?
cat "$RUN/restore-report.txt" 2>/dev/null; echo "run folder: $RUN"
# The restore is attempted whatever happens above; the script reports success only if it is proven.
[ "$SRC" -eq 0 ] || echo "note: the CCStudio session exited $SRC"
if [ ! -f "$RUN/restore-report.txt" ] || grep -q "RESTORE ERROR\|no backup was taken\|DIFFERS FROM BACKUP" "$RUN/restore-report.txt" \
   || ! grep -q "READBACK MATCHES BACKUP" "$RUN/restore-report.txt"; then
  echo "BOARD NOT PROVEN RESTORED (session exit $SRC; see $RUN/restore-report.txt and $RUN/session.out). Restore it by hand before using the board."
  exit 7
fi
[ "$MRC" -eq 0 ] || { echo "The join did not succeed (mac_side exit $MRC); the board was restored. See $RUN/mac_side.txt."; exit 1; }
