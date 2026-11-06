#!/bin/bash
# Flash a pqc.out to the LP-MSPM0G5187 (probe MG020001) with DSLite and read the RAM log with OpenOCD.  ./run_pqc.sh <pqc.out> <log out>
# No serial path: the image leaves pq.magic = 0x50510001 and the text in pqlog when it is done.
set -e
IMG=$1; LOG=$2; HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=${CFG:-$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg}; PROBE=${PROBE:?set PROBE to the probe serial}; DSF=${DSF:-$REPO/mspm0g5187/bench/dsflash.sh}   # other board: PROBE, CFG, DSF
NM=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
PQ=$($NM "$IMG" | awk '$3=="pq" {print $1}'); LG=$($NM "$IMG" | awk '$3=="pqlog" {print $1}')
# coordinator rule 2026-10-17: flash only when the 1-minute load average is under 25
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge 25 ]; do sleep 20; done
"$DSF" "$IMG" || exit 1
ocd() { "$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial $PROBE" -c init "$@" -c shutdown 2>&1; }
# One OpenOCD session for the whole run: attaching resets this chip, so every extra attach restarts the image.
WAIT=${WAIT:-40000}
ocd -c "reset run" -c "sleep $WAIT" -c halt -c "mdw 0x$PQ 6" -c "dump_image $LOG.bin 0x$LG ${LOGSZ:-12288}" | grep "^0x" > "$LOG.pq"
grep -q 50510001 "$LOG.pq" || { cat "$LOG.pq"; echo "image not finished after ${WAIT} ms; rerun with a longer WAIT"; exit 1; }
cat "$LOG.pq"; tr -d '\r\000' < "$LOG.bin" > "$LOG"; rm -f "$LOG.bin"; cat "$LOG"
