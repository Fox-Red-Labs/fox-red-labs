#!/bin/bash
# Build one variant of the bare-metal CC1354P10 benchmark, load it with CCStudio scripting (no power
# cycle needed on this board), and capture the serial report until "BENCH DONE" or 240 s.
#   ./run.sh O3 | Oz
set -uo pipefail
VAR="${1:-O3}"; HERE="$(cd "$(dirname "$0")" && pwd)"; B="$HERE/build-$VAR"; mkdir -p "$B" "$HERE/results"
make -C "$B" -f ../makefile OPT="-$VAR" SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16 \
  SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS \
  > "$HERE/results/build-$VAR.txt" 2>&1 || { tail -20 "$HERE/results/build-$VAR.txt"; exit 1; }
python3 - "$HERE/results/run-$VAR.txt" <<'PY' &
import os, select, sys, termios, time
fd = os.open("/dev/cu.usbmodemLS45013A1", os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
buf, end = b"", time.time() + 240
while time.time() < end and b"BENCH DONE" not in buf:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r: buf += os.read(fd, 4096)
open(sys.argv[1], "wb").write(buf)
PY
L=$!; sleep 1
/Applications/ti/ccs2101/ccs/scripting/python/run.sh "$HERE/../tools/ccs_load_run.py" "$B/bench.out" > "$HERE/results/load-$VAR.txt" 2>&1
wait $L
# Guard: the board must print the flags we built. A failed load leaves the previous image running and
# it prints its own report (seen 2026-10-06: an "-Oz" run that was really the -O3 image).
grep -q "Compiler flags   : -$VAR " "$HERE/results/run-$VAR.txt" || { echo "WRONG IMAGE: the board did not print -$VAR. Discard this run."; exit 3; }
tr -d '\r' < "$HERE/results/run-$VAR.txt"
