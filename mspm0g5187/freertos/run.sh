#!/bin/bash
# Flash one FreeRTOS image to the LP-MSPM0G5187 (probe MG020001 only), let it run, read the result symbol
# and the fault record over OpenOCD, save them. Usage: ./run.sh build-bench-80mhz-msp512 bench fr_bench 43 [wait ms]
#   <build dir> <name> <result symbol> <words> 
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; D="$HERE/$1"; N=$2; SYM=$3; W=$4; MS=${5:-5000}
NM=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
A=$($NM "$D/$N.out" | awk -v s=$SYM '$3==s {print $1}'); F=$($NM "$D/$N.out" | awk '$3=="fr_fault" {print $1}')
[ -n "$A" ] && [ -n "$F" ] || { echo "no symbol"; exit 1; }
"$HERE/../bench/dsflash.sh" "$D/$N.out" || exit 1
OCDB=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg
"$OCDB/bin/openocd" -s "$OCDB/openocd/scripts" -f "$CFG" -c "adapter serial ${PROBE:?set PROBE to the probe serial}" -c init -c "reset run" -c "sleep $MS" -c halt -c "mdw 0x$A $W" -c "mdw 0x$F 4" -c resume -c shutdown 2>&1 | grep -E "^0x" > "$D/$N.mdw.txt"
cat "$D/$N.mdw.txt"
