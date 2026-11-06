#!/bin/bash
# Flash maxtasks/edge-heap.out to the LP-MSPM0G3519 (probe MG000001), run it in ONE OpenOCD session (attaching resets the chip), read struct fr_edge (12 words).
#   ./run_maxtasks.sh <edge-heap.out> <out txt>
set -e
IMG=$1; OUTF=$2; HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3519/support/openocd.cfg
NM=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
A=$($NM "$IMG" | awk '$3=="fr_edge" {print $1}')
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge 25 ]; do sleep 20; done
"$REPO/mspm0g3519/bench/dsflash.sh" "$IMG" || exit 1
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial ${PROBE:?set PROBE to the probe serial}" -c init -c "reset run" -c "sleep ${WAIT:-5000}" -c halt -c "mdw 0x$A 12" -c resume -c shutdown 2>&1 | grep "^0x" | tee "$OUTF"
