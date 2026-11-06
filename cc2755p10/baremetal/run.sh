#!/bin/bash
# Build one variant of the bare-metal CC2755P10 benchmark and, unless --build-only, flash it and
# capture the serial report.
#   ./run.sh O3 --build-only    build and CCFG safety check only; touches no hardware
#   ./run.sh Oz                 build, check, flash with UniFlash dslite, capture serial
# Writes build-<variant>/ (outputs, git-ignored), results/build-<variant>.txt and, when flashing,
# results/run-<variant>.txt (serial text) and results/run-<variant>.stamps.txt (host time per line).
set -euo pipefail
VAR="${1:-}"; case "$VAR" in O3|Oz) ;; *) echo "usage: $0 O3|Oz [--build-only]"; exit 2;; esac
BUILD_ONLY=0; [ "${2:-}" = "--build-only" ] && BUILD_ONLY=1
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"; BUILD="${RUN:-$PWD/out/cc2755p10}/coremark/build-$VAR"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
DSLITE=/Applications/ti/uniflash_9.6.0/dslite.sh
CCXML="$HERE/../CC2755P10.ccxml"
SERIAL_DEV="${SERIAL_DEV:-/dev/cu.usbmodemLS5600RE1}"
mkdir -p "$BUILD" "$HERE/results"

make -C "$BUILD" -f "$HERE/makefile" SRC="$HERE" REPO="$REPO" OPT="-$VAR" \
  SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" \
  > "$HERE/results/build-$VAR.txt" 2>&1 || { tail -30 "$HERE/results/build-$VAR.txt"; exit 1; }
HEX="$BUILD/bench.hex"; [ -f "$HEX" ] || { echo "No $HEX"; exit 4; }
awk '/MEMORY CONFIGURATION/,/SEGMENT ALLOCATION MAP/' "$BUILD/bench.map" | grep -E "^ +(name|FLASH|SRAM|CCFG)" | tee -a "$HERE/results/build-$VAR.txt"

# Safety: the CCFG must keep the debug port, chip erase, flash program and return-to-factory open.
CFG="$BUILD/ti_devices_config.c"
for p in allowDebugPort allowChipErase allowFlashProgram allowReturnToFactory; do
  grep -qE "\.$p[[:space:]]*=[[:space:]]*CCFG_PERMISSION_ALLOW" "$CFG" || { echo "CCFG: $p is not CCFG_PERMISSION_ALLOW. Not flashing."; exit 4; }
done
echo "CCFG OK: debug port, chip erase, flash program and return to factory all allowed." | tee -a "$HERE/results/build-$VAR.txt"
[ "$BUILD_ONLY" = 1 ] && { echo "--build-only: not flashing. Image: $HEX"; exit 0; }

[ -e "$SERIAL_DEV" ] || { echo "$SERIAL_DEV not present. Is the board plugged in?"; exit 3; }
# Listen first, then flash with -u (run after load), so the run is captured from its first line.
# A run file exists only if it is the complete output of this build: drop the old ones first.
rm -f "$HERE/results/run-$VAR.txt" "$HERE/results/run-$VAR.stamps.txt" "$HERE/results/run-$VAR.txt.incomplete" "$HERE/results/run-$VAR.stamps.txt.incomplete"
BENCH_VAR="$VAR" python3 - "$SERIAL_DEV" "$HERE/results/run-$VAR.txt" "$HERE/results/run-$VAR.stamps.txt" <<'PY' &
import os, sys, termios, time, select
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
buf = b""; t0 = time.time(); stamps = []; partial = b""
# Wait for a BENCH DONE that follows this variant's own optimization line; the previous program may still be repeating its results.
want = ("I,optimization,-" + os.environ.get("BENCH_VAR", "")).encode()
def done(b):
    k = b.rfind(want)
    return k >= 0 and b"BENCH DONE" in b[k:]
while time.time() - t0 < 300 and not done(buf):
    if select.select([fd], [], [], 0.5)[0]:
        chunk = os.read(fd, 4096); now = time.time(); buf += chunk; partial += chunk
        while b"\n" in partial:
            line, partial = partial.split(b"\n", 1)
            stamps.append(f"{now - t0:.3f} {line.decode(errors='replace').strip()}")
text = buf.decode(errors="replace").replace("\r", "")
k = text.rfind("I,optimization,-" + os.environ.get("BENCH_VAR", "")); i = text.rfind("BENCH START", 0, k if k >= 0 else len(text)); text = text[i:] if i >= 0 else text
j = text.find("BENCH DONE"); text = text[:j + 10] + "\n" if j >= 0 else text
ok = "BENCH DONE" in text
sfx = "" if ok else ".incomplete"
open(sys.argv[2] + sfx, "w").write(text)
open(sys.argv[3] + sfx, "w").write("\n".join(stamps) + "\n")
print(f"captured {len(text.splitlines())} lines in {time.time() - t0:.0f} s" + ("" if ok else "  (TIMED OUT before BENCH DONE; partial output saved as .incomplete)"))
sys.exit(0 if ok else 1)
PY
CAP=$!
sleep 1
if ! "$DSLITE" --mode flash -c "$CCXML" -e -f -u "$HEX" > "$BUILD/flash.log" 2>&1; then
  tail -15 "$BUILD/flash.log"
  kill $CAP 2>/dev/null || true; wait $CAP 2>/dev/null || true
  rm -f "$HERE/results/run-$VAR.txt" "$HERE/results/run-$VAR.stamps.txt" "$HERE/results/run-$VAR.txt.incomplete" "$HERE/results/run-$VAR.stamps.txt.incomplete"   # whatever it heard came from the old firmware
  echo "dslite flash failed. Nothing captured (the board may still run older firmware)."; exit 5
fi
tail -15 "$BUILD/flash.log"
wait $CAP || { echo "No complete run: BENCH DONE for $VAR never appeared in 300 s."; exit 1; }
grep -E "^R,|CoreMark 1.0|Iterations/Sec|crc|Correct|Errors" "$HERE/results/run-$VAR.txt" || true
