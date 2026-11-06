#!/bin/bash
# Build CoreMark under Zephyr for the FRDM-MCXA153: ./build.sh [Os|O2|O3]  (default O2; Zephyr's speed setting)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; O=${1:-O2}; ZP=~/zephyrproject; OUT=${OUT:-$PWD/out/frdm-mcxa153/zephyr-coremark-$O}
. $ZP/.venv/bin/activate; export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-
EXTRA=""; [ $O = O2 ] && EXTRA="-DCONFIG_SPEED_OPTIMIZATIONS=y"; [ $O = O3 ] && EXTRA="-DCONFIG_SPEED_OPTIMIZATIONS=y -DEXTRA_CFLAGS=-O3"
cd $ZP/zephyr && west build -p always -b frdm_mcxa153 "$HERE" -d "$OUT" -- $EXTRA
