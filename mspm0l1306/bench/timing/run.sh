#!/bin/bash
# Flash the timing build to probe ML130001 and read struct tm.  ./run.sh <opt>
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; T="$HERE/../../tools"; OUT="$HERE/build-$1"
NM=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
A=$($NM "$OUT/timing.out" | awk '$3=="tm" {print $1}')
"$T/ocd.sh" "reset halt" "program $OUT/timing.out verify" "reset run" "sleep 1500" halt "mdw 0x$A 11" resume | grep -E "Verified|^0x" | tee "$OUT/ram.txt"
