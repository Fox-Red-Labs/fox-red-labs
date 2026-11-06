#!/bin/bash
# Flash a hex into the LP-MSPM0G3519 (probe serial PROBE, required) under the probe lock, then restart the program.
#   flash.sh <image.hex>
# Variables: OCD xpack OpenOCD folder, TC toolchain, ZW patched Zephyr workspace (for the OpenOCD config), CFG OpenOCD config,
#   DSLITE UniFlash DSLite binary, CCXML target configuration, PROBE probe serial. README: Set up once.
# 1. nRST pulse with OpenOCD: a running Zephyr image leaves the core hidden from the debugger
#    ("Could not find MEM-AP", DSLite "Connection to MSPM0 core failed").
# 2. DSLite (UniFlash 9.6.0) erase, program, verify, from a .bin padded to 8 bytes. OpenOCD's own `program` fails on a board that runs
#    Zephyr ("Unknown Device ID[0x0]"); DSLite after the pulse works.
# 3. nRST pulse again so the program starts while the console port is already open on the Mac.
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
REPO="$(cd "$(dirname "$0")/../.." && pwd)"; O="${OCD:-$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7}"
TC="${TC:-$HOME/zephyr-toolchain}"; PROBE="${PROBE:?set PROBE to the probe serial}"
ZW="${ZW:?set ZW to the Zephyr workspace with the MSPM0 patches (see README)}"
CFG="${CFG:-$ZW/zephyr/boards/ti/lp_mspm0g3519/support/openocd.cfg}"
DS="${DSLITE:-/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite}"
CCXML="${CCXML:-$REPO/mspm0g3519/MSPM0G3519_$PROBE.ccxml}"
[ -n "${1:-}" ] || { echo "usage: flash.sh <image.hex>" >&2; exit 2; }
need OCD "$O/bin/openocd"; need CFG "$CFG"; need DSLITE "$DS"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy"; need CCXML "$CCXML"; need hex "$1"
LOCK=(python3 "$REPO/tools/frl/lock.py" exec --serial "$PROBE" --op zephyr-mspm0 --)
OCD_RUN=("$O/bin/openocd" -s "$O/openocd/scripts" -c "adapter serial $PROBE" -f "$CFG")
pulse() { "${LOCK[@]}" "${OCD_RUN[@]}" -c "init" -c "mspm0_board_reset" -c "shutdown" >/dev/null 2>&1 || true; }
# DSLite writes 64 bit words and refuses a block whose length is not a multiple of 8: make a padded .bin.
BIN="$(mktemp -t zmspm0).bin"; "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy" -I ihex -O binary "$1" "$BIN"
python3 - "$BIN" <<'PY'
import sys
p = sys.argv[1]; d = open(p, "rb").read(); d += b"\xff" * (-len(d) % 8); open(p, "wb").write(d)
PY
pulse
"${LOCK[@]}" "$DS" flash -c "$CCXML" -e -f -v "$BIN,0x0"
rm -f "$BIN"
pulse
