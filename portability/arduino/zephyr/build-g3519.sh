#!/bin/bash
# Usage: build-g3519.sh Blink|AnalogReadSerial   (unchanged Arduino sketches on Zephyr main 4.5.0-dev, LP-MSPM0G3519)
# Zephyr main is needed because 4.4.2 has no MSPM0 ADC driver. ZM = a Zephyr main checkout (read only here),
# HALTI = its hal_ti module, CMSIS6 = the cmsis_6 module.
# ZW = a patched west workspace (portability/zephyr-mspm0/README.md: Zephyr main 1405adc9a plus the MSPM0 I2C pull
# request #117311, which gives Wire a driver). When set, ZM, HALTI and CMSIS6 come from it.
#   ZW=<zephyr workspace> OVERLAY=<overlay> CONF=<extra .conf> build-g3519.sh <sketch>
# Variables (all optional, defaults are the author's machine; a missing path stops the script with one line naming the variable):
#   AM ArduinoCore-zephyr checkout, ZW/ZM/HALTI/CMSIS6 Zephyr workspace, PICOLIBC picolibc module, TC toolchain, VENV python venv with west, OUT build folder.
# SKETCH_DIR = folder holding <sketch>/<sketch>.ino (default sketches/).
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
SK="${1:?sketch name}"
HERE="$(cd "$(dirname "$0")" && pwd)"
AM="${AM:-$HOME/zr-arduino}"
if [ -n "${ZW:-}" ]; then ZM="$ZW/zephyr"; HALTI="$ZW/modules/hal/ti"; CMSIS6="$ZW/modules/hal/cmsis_6"; fi
ZM="${ZM:-$HOME/zephyr-main-check/zephyr}"
HALTI="${HALTI:-$HOME/zephyr-main-check/modules/hal/ti}"
CMSIS6="${CMSIS6:-$HOME/zephyr-main-check/modules/hal/cmsis_6}"
OUT="${OUT:-$PWD/out/build-ard-g3519-$SK}"
PICOLIBC="${PICOLIBC:-$HOME/zephyrproject/modules/lib/picolibc}"
TC="${TC:-$HOME/zephyr-toolchain}"; VENV="${VENV:-$HOME/zephyrproject/.venv}"
need AM "$AM"; need ZM "$ZM"; need HALTI "$HALTI"; need CMSIS6 "$CMSIS6"; need PICOLIBC "$PICOLIBC"
need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"; need OUT "$(dirname "$OUT")"
. "$VENV/bin/activate"
export ZEPHYR_BASE="$ZM" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
rm -rf "$OUT"
cmake -S "$HERE/app" -B "$OUT" -GNinja -DBOARD=lp_mspm0g3519 -DZEPHYR_MODULES="$HALTI;$CMSIS6;$PICOLIBC" \
  -DSKETCH="$SK" -DSKETCH_DIR="${SKETCH_DIR:-$HERE/sketches}" ${CONF:+-DEXTRA_CONF_FILE="$CONF"} -DZEPHYR_EXTRA_MODULES="$AM" \
  -DDTC_OVERLAY_FILE="${OVERLAY:-$HERE/variants/lp_mspm0g3519/lp_mspm0g3519.overlay}"
ninja -C "$OUT"
