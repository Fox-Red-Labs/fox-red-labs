#!/bin/bash
# Wrapper: timestamped UART capture from the CC1354P10 while ccs_session_cc1354.py runs all apps in one session.
# Usage: run_cc1354_session.sh <build dir> <restore dir> <results jsonl> <uart log>
HERE=$(cd "$(dirname "$0")" && pwd)
python3 - "$4" <<'PY' &
import os, select, sys, termios, time
fd = os.open("/dev/cu.usbmodemLS45013A1", os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
with open(sys.argv[1], "w") as f:
    while True:
        r, _, _ = select.select([fd], [], [], 0.5)
        if r:
            d = os.read(fd, 4096)
            if d: f.write(f"{time.time():.3f} {d!r}\n"); f.flush()
PY
LISTENER=$!
sleep 1
/Applications/ti/ccs2101/ccs/scripting/python/run.sh "$HERE/ccs_session_cc1354.py" "$1" "$2" "$3" 6
RC=$?
sleep 1; kill $LISTENER 2>/dev/null
exit $RC
