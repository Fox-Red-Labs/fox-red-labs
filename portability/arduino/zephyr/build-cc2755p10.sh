#!/bin/bash
# Variables: AM ArduinoCore-zephyr checkout, ZP TI Zephyr fork west workspace (simplelink-zephyr v3.7.0-ti-1.00.01_ga), TC toolchain, VENV python venv, OUT build folder (README: Set up once).
# Usage: build-cc2755p10.sh <sketch>   (a folder in sketches/; Zephyr fork 3.7.0, LP-EM-CC2755P10)
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
SK="${1:?sketch name}"
HERE="$(cd "$(dirname "$0")" && pwd)"
AM="${AM:-$HOME/zr-arduino}"
OUT="${OUT:-$PWD/out/build-ard-cc2755p10-$SK}"
ZP="${ZP:-$HOME/zephyr-ti/ws}"; TC="${TC:-$HOME/zephyr-toolchain}"
VENV="${VENV:-$ZP/../.venv}"
need AM "$AM"; need ZP "$ZP/zephyr"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"; need OUT "$(dirname "$OUT")"
. "$VENV/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
cd "$ZP/zephyr"
west build -p always -b lp_em_cc2755p10 "$HERE/app" -d "$OUT" -- \
  -DSKETCH="$SK" -DSKETCH_DIR="$HERE/sketches" -DZEPHYR_EXTRA_MODULES="$AM" \
  -DDTC_OVERLAY_FILE="$HERE/variants/lp_em_cc2755p10/lp_em_cc2755p10.overlay"
