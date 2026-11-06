#!/bin/bash
# Build Zephyr apps for the LP-MSPM0L1306 with the out-of-tree board in ./boards. Touches no hardware.
#   ./build.sh hello_world | blinky | blinky-rgb-red
#   ./build.sh bench-Os | bench-O2 | bench-O3            mcu-bench-l1306 (the version that fits 4 KB)
#   ./build.sh coremark2k-Os | coremark2k-O2 | coremark2k-O3   CoreMark alone at the standard 2000 bytes
#   ./build.sh coremark2k-trueO3                         same, with the app really compiled at -O3
#   ./build.sh bench-port-Os | bench-port-O2 | bench-port-O3   the copy with the G3507 memory settings (prj-g3507.conf)
#   ./build.sh bench-orig-Os | bench-orig-O2 | bench-orig-O3   the original G3507 mcu-bench, unchanged
#   ./build.sh synchronization | philosophers | shell_module | logger
#   ./build.sh all                                       everything above, in turn
#   ./build.sh custom <name> <app dir> [extra cmake args]
# Output: $OUT/<name>/zephyr/zephyr.hex and $OUT/<name>.log. Prints the memory report or the overflow lines.
# Env: ZP (default ~/zephyrproject), TC (default ~/zephyr-toolchain), OUT (default the session scratchpad).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
OUT="${OUT:-$PWD/out/zephyr-builds}"
BENCH_ORIG="$HERE/../../mspm0g3507/mcu-bench"
mkdir -p "$OUT"
. "$ZP/.venv/bin/activate"
export ZEPHYR_BASE="$ZP/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"

build() { # name app [cmake args...]
  local name="$1" app="$2"; shift 2
  (cd "$ZP/zephyr" && west build -p always -b lp_mspm0l1306 "$app" -d "$OUT/$name" -- \
     -DBOARD_ROOT="$HERE" "$@") > "$OUT/$name.log" 2>&1
  local rc=$?
  echo "== $name (exit $rc)"
  grep -E "^ *(FLASH|RAM):|overflowed by|will not fit|region .* overflowed" "$OUT/$name.log" | sed 's/^/   /'
  [ $rc -ne 0 ] && grep -m3 -E "error:|Error:|FATAL" "$OUT/$name.log" | sed 's/^/   /'
  [ -f "$OUT/$name/zephyr/zephyr.hex" ] && echo "   hex: $OUT/$name/zephyr/zephyr.hex"
  return $rc
}

bench_args() { # Os | O2 | O3, same switches as mcu-bench/run.sh
  case "$1" in
    Os) echo "-DBENCH_COREMARK_ITERATIONS=3000" ;;
    O2) echo "-DEXTRA_CONF_FILE=speed.conf" ;;
    O3) echo "-DEXTRA_CONF_FILE=speed.conf -DBENCH_O3=1" ;;
  esac
}

one() {
  case "$1" in
    hello_world)     build hello_world samples/hello_world ;;
    blinky)          build blinky samples/basic/blinky ;;
    blinky-rgb-red)  build blinky-rgb-red samples/basic/blinky -DEXTRA_DTC_OVERLAY_FILE="$HERE/overlays/blinky-rgb-red.overlay" ;;
    bench-Os|bench-O2|bench-O3)
                     build "$1" "$HERE/mcu-bench-l1306" $(bench_args "${1#bench-}") ;;
    coremark2k-trueO3) # CoreMark alone with the app really at -O3 (BENCH_TRUE_O3, see mcu-bench-l1306/CMakeLists.txt)
                     build "$1" "$HERE/mcu-bench-l1306" -DBENCH_COREMARK_ONLY=1 -DBENCH_COREMARK_DATA=2000 \
                       "-DEXTRA_CONF_FILE=speed.conf;coremark2k.conf" -DBENCH_O3=1 -DBENCH_TRUE_O3=1 ;;
    coremark2k-Os|coremark2k-O2|coremark2k-O3)
                     o="${1#coremark2k-}"; c="coremark2k.conf"; [ "$o" != Os ] && c="speed.conf;coremark2k.conf"
                     x=""; [ "$o" = O3 ] && x="-DBENCH_O3=1"
                     build "$1" "$HERE/mcu-bench-l1306" -DBENCH_COREMARK_ONLY=1 -DBENCH_COREMARK_DATA=2000 \
                       "-DEXTRA_CONF_FILE=$c" $x ;;
    bench-port-Os|bench-port-O2|bench-port-O3)
                     build "$1" "$HERE/mcu-bench-l1306" -DCONF_FILE=prj-g3507.conf $(bench_args "${1#bench-port-}") ;;
    bench-orig-Os|bench-orig-O2|bench-orig-O3)
                     build "$1" "$BENCH_ORIG" $(bench_args "${1#bench-orig-}") ;;
    synchronization) build synchronization samples/synchronization ;;
    philosophers)    build philosophers samples/philosophers ;;
    shell_module)    build shell_module samples/subsys/shell/shell_module ;;
    logger)          build logger samples/subsys/logging/logger ;;
    *) echo "unknown target: $1"; return 2 ;;
  esac
}

case "${1:-}" in
  all) for t in hello_world blinky blinky-rgb-red bench-Os bench-O2 bench-O3 coremark2k-Os coremark2k-O2 coremark2k-O3 coremark2k-trueO3 bench-port-Os bench-port-O2 bench-port-O3 bench-orig-Os bench-orig-O2 bench-orig-O3 \
                synchronization philosophers shell_module logger; do one "$t"; done ;;
  custom) shift; n="$1"; a="$2"; shift 2
          [ -d "$a" ] && a="$(cd "$a" && pwd)"   # a folder here, else a path inside the Zephyr tree
          build "$n" "$a" "$@" ;;
  "") sed -n '2,14p' "$0"; exit 2 ;;
  *) one "$1" ;;
esac
