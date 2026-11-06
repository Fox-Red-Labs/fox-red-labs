#!/bin/bash
# Build MicroPython v1.29.0 (ports/zephyr) for the LP-EM-CC2340R5 with upstream Zephyr 4.4.2.
# Variables: MP MicroPython checkout, ZP Zephyr 4.4.2 west workspace, TC toolchain, VENV python venv, OUT build folder (README: Set up once).
# MP = a MicroPython v1.29.0 checkout (shallow clone is enough), OUT = build folder. Disk: about 60 MB.
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
HERE="$(cd "$(dirname "$0")" && pwd)"
MP="${MP:-$HOME/micropython}"; OUT="${OUT:-$PWD/out/build-mp-cc2340r5}"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
VENV="${VENV:-$ZP/.venv}"
need MP "$MP/ports/zephyr"; need ZP "$ZP/zephyr"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"; need OUT "$(dirname "$OUT")"
. "$VENV/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
cd "$ZP/zephyr"
west build -p always -b lp_em_cc2340r5 "$MP/ports/zephyr" -d "$OUT" -- \
  -DCONF_FILE="$HERE/prj_cc2340r5.conf" -DEXTRA_DTC_OVERLAY_FILE="$HERE/lp_em_cc2340r5.overlay"
