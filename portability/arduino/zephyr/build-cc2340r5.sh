#!/bin/bash
# Variables: AM ArduinoCore-zephyr checkout, ZP Zephyr 4.4.2 west workspace, TC toolchain, VENV python venv, OUT build folder (README: Set up once).
# Usage: build-cc2340r5.sh <sketch>   (a folder in sketches/: Blink, AnalogReadSerial, InterruptSelfTest, FadeCheck, SpiLoop, WireScan; Zephyr 4.4.2, LP-EM-CC2340R5)
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
SK="${1:?sketch name}"
HERE="$(cd "$(dirname "$0")" && pwd)"
AM="${AM:-$HOME/zr-arduino}"
OUT="${OUT:-$PWD/out/build-ard-cc2340r5-$SK}"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
VENV="${VENV:-$ZP/.venv}"
need AM "$AM"; need ZP "$ZP/zephyr"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"; need OUT "$(dirname "$OUT")"
. "$VENV/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
cd "$ZP/zephyr"
west build -p always -b lp_em_cc2340r5 "$HERE/app" -d "$OUT" -- \
  -DSKETCH="$SK" -DSKETCH_DIR="$HERE/sketches" -DZEPHYR_EXTRA_MODULES="$AM" \
  -DDTC_OVERLAY_FILE="$HERE/variants/lp_em_cc2340r5/lp_em_cc2340r5.overlay"
