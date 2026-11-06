#!/bin/bash
# Build every Thread-Metric test that fits, one image each, from ONE build folder.
# The tests differ only in app code (no Kconfig change), so Zephyr and the C library compile once.
# Usage: ./build_all.sh [tests...]   (default: all eight)   Env: OUT, ZP, TC, TM_DURATION, TM_STACK
# Output: $OUT/tm-<test>.hex and a size line per test.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; BR="$HERE/../.."
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
OUT="${OUT:-$PWD/out/zb2}"
mkdir -p "$OUT"; . "$ZP/.venv/bin/activate"
export ZEPHYR_BASE="$ZP/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
TESTS=("$@"); [ ${#TESTS[@]} -eq 0 ] && TESTS=(basic cooperative preemptive interrupt interrupt_preemption memory message synchronization)
for t in "${TESTS[@]}"; do
  # The two interrupt tests need CONFIG_TEST (irq.conf), so they use their own build folder.
  D="$OUT/tm"; X=""; case $t in interrupt|interrupt_preemption) D="$OUT/tm-irq"; X="-DEXTRA_CONF_FILE=$HERE/irq.conf";; esac
  (cd "$ZP/zephyr" && west build -b lp_mspm0l1306 "$HERE" -d "$D" -- -DBOARD_ROOT="$BR" $X \
     -DCONF_FILE="$HERE/prj.conf" -DTM_TEST=$t ${TM_DURATION:+-DTM_DURATION=$TM_DURATION} ${TM_STACK:+-DTM_STACK=$TM_STACK}) > "$OUT/tm-$t.log" 2>&1
  rc=$?
  echo "== tm-$t (exit $rc)"
  grep -E "^ *(FLASH|RAM):|overflowed by|will not fit" "$OUT/tm-$t.log" | sed 's/^/   /'
  [ $rc -ne 0 ] && grep -m3 -E "error:|Error:|FATAL" "$OUT/tm-$t.log" | sed 's/^/   /'
  [ $rc -eq 0 ] && cp "$D/zephyr/zephyr.hex" "$OUT/tm-$t.hex" && cp "$D/zephyr/zephyr.map" "$OUT/tm-$t.map" && echo "   hex: $OUT/tm-$t.hex"
done
