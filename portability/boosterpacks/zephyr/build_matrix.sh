#!/bin/bash
# Build every shield x board x sample and print one result line each.
# Not a flash script: nothing here touches a board.
#
# Needs: a Zephyr workspace with a west venv, the arm-zephyr-eabi toolchain,
# and the HAL modules. Set these (defaults are this lab's paths):
#   ZEPHYR_WS     west workspace top (the folder that holds .west)
#   ZEPHYR_BASE   the zephyr checkout to build with
#   HAL_TI        hal_ti checkout that matches that Zephyr
#   HAL_CMSIS6    hal cmsis_6 checkout
#   HAL_CMSIS     hal cmsis checkout
#   OUT           folder for build trees and logs
# Usage: build_matrix.sh <tag> [boards...]   (tag is "main" or "rel")
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TAG=${1:?tag}; shift
BOARDS=${*:-"lp_mspm0g3519 lp_mspm0g3507 lp_mspm33c321a lp_em_cc2340r5"}
OUT=${OUT:-$HOME/boosterpack-zephyr-builds}
. "${ZEPHYR_VENV:-$HOME/zephyrproject/.venv}/bin/activate"
export ZEPHYR_BASE ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=${CROSS_COMPILE:-$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-}
MODS="$HAL_TI;$HAL_CMSIS6;$HAL_CMSIS;$HERE"
mkdir -p "$OUT"
run() { # shield board sample
  local sh=$1 b=$2 smp=$3 n; n=${TAG}-${sh}-${b}-$(basename "$smp")
  rm -rf "$OUT/$n"
  if (cd "$ZEPHYR_WS" && west build -p always -b "$b" "$ZEPHYR_BASE/$smp" -d "$OUT/$n" \
        -- -DZEPHYR_EXTRA_MODULES="$MODS" -DSHIELD="$sh" >"$OUT/$n.log" 2>&1); then
    local sz; sz=$(grep -E "^ *(FLASH|RAM):" "$OUT/$n.log" | tr -s ' ' | tr '\n' ';')
    echo "PASS,$TAG,$sh,$b,$smp,$sz"
  else
    local err; err=$(grep -m1 -E "devicetree error|error:|undefined reference|Error|ERROR" "$OUT/$n.log" | cut -c1-300)
    echo "FAIL,$TAG,$sh,$b,$smp,$err"
  fi
  rm -rf "$OUT/$n"
}
for b in $BOARDS; do
  run boostxl_edumkii "$b" samples/drivers/display
  run boostxl_edumkii "$b" samples/subsys/input/input_dump
  run boostxl_sharp128 "$b" samples/drivers/display
  run boostxl_audio "$b" samples/drivers/dac
  [ "$b" = lp_mspm0g3507 ] && run boostxl_edumkii_gpiospi "$b" samples/drivers/display
done
