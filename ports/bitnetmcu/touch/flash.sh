#!/bin/bash
# Flash the touch digit demo onto the LP-MSPM0G3507 with OpenOCD and start it. Needs the board on USB.
#   ./flash.sh                 flashes out/digit_touch.out (build it first with ./build.sh)
#   ./flash.sh path/to/x.out   flashes that file instead
#   ./flash.sh --check         only checks tools, image and probe setting; does not open the probe
# Settings come from the environment (names shared with the other scripts, see ../recipe-env.sh):
#   PROBE   REQUIRED. Your LaunchPad kit's XDS110 serial: the text after "usbmodem" in `ls /dev/cu.usbmodem*` minus the
#           last digit. The script never guesses a probe, so it cannot flash a board that is not yours.
#   OCD     the xPack OpenOCD folder (default ~/zephyr-toolchain/xpack-openocd-0.12.0-7)
#   OCD_CFG the OpenOCD connection file (default ../openocd-mspm0.cfg, tracked here)
#   OUT     where flash.txt goes (default ./out)
# Paths may contain spaces. On this board OpenOCD's verify fails on about every other flash and passes on the next try,
# so it tries up to three times.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../recipe-env.sh"
OUT="${OUT:-$HERE/out}"
CHECK=0; [ "$1" = "--check" ] && { CHECK=1; shift; }
IMG="${1:-$OUT/digit_touch.out}"
br_need_path "image" "$IMG" "Build it first with ./build.sh, or pass the path of an .out file."
br_need_exec "OCD" "$OCD/bin/openocd" "Install xPack OpenOCD 0.12.0 (https://github.com/xpack-dev-tools/openocd-xpack/releases), unpack it, and point OCD at the folder that holds bin/openocd and openocd/scripts, e.g. export OCD=\"\$HOME/xpack-openocd-0.12.0-7\"."
br_need_path "OCD scripts" "${OCD_SCRIPTS:-$OCD/openocd/scripts}" "OCD must be the xPack folder (it has openocd/scripts inside)."
br_need_path "OCD_CFG" "$OCD_CFG" "OCD_CFG is the OpenOCD connection file; the default ships in this repository."
br_need_probe
[ $CHECK = 1 ] && { br_report; exit 0; }
br_report quiet
mkdir -p "$OUT"
for try in 1 2 3; do
  if "$OCD/bin/openocd" -s "${OCD_SCRIPTS:-$OCD/openocd/scripts}" -f "$OCD_CFG" -c "adapter serial $PROBE" -c init -c "reset halt" \
       -c "program {$IMG} verify" -c "reset run" -c shutdown > "$OUT/flash.txt" 2>&1; then
    grep -E "Verified OK" "$OUT/flash.txt" | tail -1; echo "running: the screen should say DRAW A DIGIT"; exit 0
  fi
  echo "flash attempt $try failed: $(grep -m1 -E 'Verify Failed|rror|no device' "$OUT/flash.txt")"
done
echo "gave up after 3 tries; the full log is $OUT/flash.txt"; exit 1
