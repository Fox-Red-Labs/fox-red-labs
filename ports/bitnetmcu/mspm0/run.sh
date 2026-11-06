#!/bin/bash
# Flash one build, fill RAM with 0xA5 before any code runs, let it classify the 10 digits, read the answers.
#   ./run.sh <build folder> [seconds, default 20]
#   ./run.sh --check [build folder | g3507 | l1306]   only checks tools and settings; never opens the probe
# Settings come from the environment (names shared with the other scripts, see ../recipe-env.sh). Set them first:
#   PROBE   REQUIRED. Your LaunchPad kit's XDS110 serial: the text after "usbmodem" in `ls /dev/cu.usbmodem*` minus the
#           last digit. The script never guesses a probe, so it cannot flash a board that is not yours.
#   OCD     the xPack OpenOCD folder (default ~/zephyr-toolchain/xpack-openocd-0.12.0-7)
#   OCD_CFG the OpenOCD connection file (default ../openocd-mspm0.cfg, tracked here)
#   NM      the symbol tool (default $CC/bin/tiarmnm from TI Arm Clang; the output is the same as GNU nm's)
#   CC      TI Arm Clang folder, only used to find NM
#   XDSDFU  L1306 only: TI's xdsdfu tool, used to check the probe is present before OpenOCD starts
# Paths may contain spaces. This script calls the bench helpers mspm0g3507/mcu-bench/sdk-common/ocd.sh (G3507)
# and mspm0l1306/tools/ocd.sh (L1306), passing PROBE, OCD and OCD_CFG to them.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../../.."
. "$HERE/../recipe-env.sh"
CHECK=0; [ "$1" = "--check" ] && { CHECK=1; shift; }
ARG="${1:-}"; S=${2:-20}
case "$ARG" in
  g3507) KIND=g3507 ;;
  l1306) KIND=l1306 ;;
  "") [ $CHECK = 1 ] && KIND=any || { echo "usage: ./run.sh <build folder> [seconds]   or   ./run.sh --check [g3507|l1306]"; exit 2; } ;;
  *) D="$(cd "$ARG" 2>/dev/null && pwd)" || { echo "build folder not found: $ARG (build it first with ./build.sh)"; exit 2; }
     case "$D" in
       *build-g3507-*) KIND=g3507 ;;
       *build-l1306-*) KIND=l1306 ;;
       *) echo "the folder name must contain build-g3507- or build-l1306- (as ./build.sh names it): $D"; exit 2 ;;
     esac ;;
esac
case $KIND in
  g3507|any) OCDSH="$R/mspm0g3507/mcu-bench/sdk-common/ocd.sh"; RAM0=0x20200000; RAMW=8192 ;;
  l1306) OCDSH="$R/mspm0l1306/tools/ocd.sh"; RAM0=0x20000000; RAMW=1024 ;;
esac
[ -n "${D:-}" ] && br_need_path "build" "$D/bitnet.out" "Build it first with ./build.sh <board> <model> <opt> <clk>."
br_need_exec "NM" "$NM" "NM is a symbol tool (nm). TI Arm Clang ships one as bin/tiarmnm, so set CC to your TI Arm Clang folder (export CC=\"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS\") or set NM to any nm that can read an Arm ELF file."
br_need_exec "OCD" "$OCD/bin/openocd" "Install xPack OpenOCD 0.12.0 (https://github.com/xpack-dev-tools/openocd-xpack/releases), unpack it, and point OCD at the folder that holds bin/openocd and openocd/scripts, e.g. export OCD=\"\$HOME/xpack-openocd-0.12.0-7\"."
br_need_path "OCD scripts" "${OCD_SCRIPTS:-$OCD/openocd/scripts}" "OCD must be the xPack folder (it has openocd/scripts inside)."
br_need_path "OCD_CFG" "$OCD_CFG" "OCD_CFG is the OpenOCD connection file; the default ships in this repository."
br_need_exec "ocd helper" "$OCDSH" "This script is part of the repository; re-clone it."
br_need_cmd "python3" python3 "Install Python 3 (it reads the answers back)."
br_need_cmd "awk" awk "awk ships with macOS and Linux."
br_need_probe
if [ $CHECK = 1 ]; then br_report; exit 0; fi
br_report quiet
export PROBE OCD OCD_CFG; [ -n "${OCD_SCRIPTS:-}" ] && export OCD_SCRIPTS; [ -n "${XDSDFU:-}" ] && export XDSDFU
A=$("$NM" "$D/bitnet.out" | awk '$3=="bnm" {print $1}')
[ -n "$A" ] || { echo "no symbol bnm in $D/bitnet.out (is NM able to read an Arm ELF file?)"; exit 2; }
# on the G3507, OpenOCD's verify fails on about every other flash; the next attempt passes, so try up to 3 times
for try in 1 2 3; do
  rc=0; "$OCDSH" "reset halt" "program {$D/bitnet.out} verify" > "$D/flash.txt" 2>&1 || rc=$?
  [ $rc = 0 ] && break
  [ $rc = 3 ] && { cat "$D/flash.txt"; exit 3; }   # the helper says the probe is not on the USB bus: retrying will not help
  echo "flash attempt $try: $(grep -m1 -E 'Verify Failed|rror|no device' "$D/flash.txt")"; [ $try = 3 ] && exit 1
done; grep -E "Verified OK" "$D/flash.txt" | tail -1
"$OCDSH" "reset halt" "mww $RAM0 0xA5A5A5A5 $RAMW" resume "sleep $((S * 1000))" halt "mdw 0x$A 24" \
  "dump_image {$D/sram.bin} $RAM0 $((RAMW * 4))" resume | grep -E "^0x" | tee "$D/ram.txt"
python3 - "$D/ram.txt" "$D/sram.bin" "$D/bitnet.map" <<'PY'
import sys, re
w = [int(x, 16) for l in open(sys.argv[1]) for x in l.split(':')[1].split()]
magic, clk, ovh, right = w[:4]; pred, cyc = w[4:14], w[14:24]
print(f"magic {magic:#x} ({'done' if magic == 0xB17E0001 else 'NOT DONE'}), clock {clk} Hz, right {right} of 10")
print("answers", "".join(str(p) for p in pred), " cycles", min(cyc), "to", max(cyc), f"(mean {sum(cyc)//10:,}; {sum(cyc)/10/clk*1000:.2f} ms)")
d = open(sys.argv[2], 'rb').read(); m = open(sys.argv[3]).read()
lo, n = [int(x, 16) for x in re.search(r'^\.stack\s+0\s+([0-9a-f]{8})\s+([0-9a-f]{8})', m, re.M).groups()]
base = int(re.search(r'SRAM\s+([0-9a-f]{8})', m).group(1), 16)
used = next(i for i in range(lo - base, lo - base + n, 4) if d[i:i + 4] != b'\xa5' * 4)
print(f"stack: {n} bytes reserved, deepest use {lo + n - base - used} bytes")
PY
