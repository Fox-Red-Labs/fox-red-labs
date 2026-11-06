#!/bin/bash
# Generate the Blinky and all-properties services for Zephyr and compile and link them in a Bluetooth
# peripheral for lp_mspm0g3507 (upstream Zephyr, host only, controller on UART0). Not for flashing.
#   tools/gatt_gen/zephyr_check/build.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WS=${WS:-$HOME/zephyrproject}
BUILD=${BUILD:-$WS/build-gatt-gen-check}
GEN="$BUILD/gen"
mkdir -p "$GEN"
python3 "$HERE/../gatt_gen.py" "$HERE/../blinky.json" --target zephyr --out "$GEN"
python3 "$HERE/../gatt_gen.py" "$HERE/../examples/all_properties.json" --target zephyr --out "$GEN"
source "$WS/.venv/bin/activate"
export ZEPHYR_BASE="$WS/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=${ZEPHYR_TOOLCHAIN_VARIANT:-cross-compile}
export CROSS_COMPILE=${CROSS_COMPILE:-$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-}
west build -p always -b lp_mspm0g3507 -d "$BUILD/app" "$HERE" -- -DGEN_DIR="$GEN"
