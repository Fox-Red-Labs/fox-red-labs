#!/bin/bash
# Build MicroPython v1.29.0 (ports/zephyr) for the LP-MSPM33C321A with Zephyr main plus open TI pull requests.
# Variables: MP MicroPython checkout, ZW Zephyr workspace (README: Set up once), TC toolchain, VENV python venv with west,
# PICO picolibc module folder, OUT build folder, KIND uart|i2c|spi. MicroPython's own prj.conf is used unchanged, plus mspm33c321a.conf.
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
HERE="$(cd "$(dirname "$0")" && pwd)"
MP="${MP:-$HOME/micropython}"; ZW="${ZW:-$HOME/zephyr-mspm33}"
TC="${TC:-$HOME/zephyr-toolchain}"; VENV="${VENV:-$HOME/zephyrproject/.venv}"
PICO="${PICO:-$ZW/modules/lib/picolibc}"; OUT="${OUT:-$HOME/build-mp-mspm33}"
KIND="${KIND:-uart}"   # uart, i2c or spi: the kind you gave zephyr-tree.sh; picks overlay-$KIND.overlay
OVERLAY="$HERE/overlay-$KIND.overlay"; EXTRA="$HERE/mspm33c321a.conf"
need MP "$MP/ports/zephyr"; need ZW "$ZW/zephyr/west.yml"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"; need PICO "$PICO/CMakeLists.txt"
. "$VENV/bin/activate"
export ZEPHYR_BASE="$ZW/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-" GIT_OPTIONAL_LOCKS=0
ARGS=(-DZEPHYR_EXTRA_MODULES="$PICO;$ZW/modules/hal/cmsis_6" -DEXTRA_DTC_OVERLAY_FILE="$OVERLAY" -DEXTRA_CONF_FILE="$EXTRA")
cd "$ZW/zephyr"
west build -p always -b lp_mspm33c321a "$MP/ports/zephyr" -d "$OUT" -- "${ARGS[@]}"
