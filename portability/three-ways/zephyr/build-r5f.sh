#!/bin/bash
# Three ways, BeagleY-AI MCU R5F (beagley_ai/j722s/mcu_r5f0_0, upstream board). Run on the Mac mini:
#   ./build-r5f.sh        (source in /Volumes/zephyr/frl-r5f/three-ways, Zephyr in /Volumes/zephyr/zephyr-am62l)
# Output: build-r5f/zephyr/zephyr.elf, to be copied to /lib/firmware/j722s-mcu-r5f0_0-fw on the board.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
V=/Volumes/zephyr
. "$V/zephyr-am62l/.venv/bin/activate"
export ZEPHYR_BASE="$V/zephyr-am62l/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE="$V/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
O="$HERE/build-r5f"
rm -rf "$O"
(cd "$ZEPHYR_BASE" && west build -p always -b beagley_ai/j722s/mcu_r5f0_0 "$HERE" -d "$O") > "$HERE/build-r5f.log" 2>&1
rc=$?
grep -E "^ *(FLASH|RAM|SRAM|DRAM|TRACE|RSC_TABLE|ATCM|BTCM):|overflowed|error|Error" "$HERE/build-r5f.log" | head -20
exit $rc
