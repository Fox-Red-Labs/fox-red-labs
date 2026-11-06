#!/bin/bash
# Three ways, Zephyr 4.4.2: clean build for one board. Usage: ./build.sh g3507|l1306
# g3507 uses upstream lp_mspm0g3507; l1306 uses this repo's out-of-tree board (mspm0l1306/zephyr).
# Output: build-<board>/zephyr/zephyr.hex (git-ignored); map, memory report and log copied to ../maps and ../logs.
set -uo pipefail
B="${1:?board: g3507 or l1306}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
. "$ZP/.venv/bin/activate"
export ZEPHYR_BASE="$ZP/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
case "$B" in
  g3507) BOARD=lp_mspm0g3507; EXTRA=() ;;
  l1306) BOARD=lp_mspm0l1306; EXTRA=(-DBOARD_ROOT="$HERE/../../../mspm0l1306/zephyr") ;;
  *) echo "unknown board $B"; exit 2 ;;
esac
O="$HERE/build-$B"; LOG="$HERE/../logs/zephyr-$B.log"
rm -rf "$O"
t0=$(python3 -c 'import time;print(time.time())')
(cd "$ZP/zephyr" && west build -p always -b "$BOARD" "$HERE" -d "$O" -- ${EXTRA[@]+"${EXTRA[@]}"}) > "$LOG" 2>&1
rc=$?
t1=$(python3 -c 'import time;print(time.time())')
echo "== clean build $(python3 -c "print(round($t1-$t0,1))") s, exit $rc, $(sysctl -n hw.ncpu) cores (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
grep -E "^ *(FLASH|RAM|SRAM):|overflowed by|error:" "$LOG"
[ $rc -eq 0 ] && cp "$O/zephyr/zephyr.map" "$HERE/../maps/zephyr-$B.map"
exit $rc
