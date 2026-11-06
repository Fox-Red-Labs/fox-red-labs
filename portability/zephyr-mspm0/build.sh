#!/bin/bash
# Build one test app for the LP-MSPM0G3519 with Zephyr main plus the MSPM0 I2C and flash drivers.
#   build.sh <flash_test|i2c_probe|i2c_loopback|i2c_target_api|hello> [OUT]
# ZW  Zephyr workspace with the patches applied (required, see README)
# TC  toolchain folder (default ~/zephyr-toolchain: Zephyr SDK 0.17.4 GCC, cross-compile variant)
# VENV python venv with west (default ~/zephyrproject/.venv)
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
HERE="$(cd "$(dirname "$0")" && pwd)"
ZW="${ZW:?set ZW to the Zephyr workspace with the MSPM0 patches (see README)}"; TC="${TC:-$HOME/zephyr-toolchain}"; VENV="${VENV:-$HOME/zephyrproject/.venv}"
APP="${1:?app: flash_test|i2c_probe|i2c_loopback|i2c_target_api|hello}"; OUT="${2:-${TMPDIR:-/tmp}/zephyr-mspm0/$APP}"
need ZW "$ZW/zephyr"; need TC "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"; need VENV "$VENV/bin/activate"
. "$VENV/bin/activate"
export ZEPHYR_BASE="$ZW/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-" GIT_OPTIONAL_LOCKS=0
case "$APP" in
  hello)          SRC="$ZW/zephyr/samples/hello_world";;
  i2c_target_api) SRC="$ZW/zephyr/tests/drivers/i2c/i2c_target_api";;
  *)              SRC="$HERE/apps/$APP";;
esac
cd "$ZW/zephyr"
west build -p "${PRISTINE:-auto}" -b lp_mspm0g3519 "$SRC" -d "$OUT" -- -DBUILD_VERSION=main-mspm0
echo "hex: $OUT/zephyr/zephyr.hex"
