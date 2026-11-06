#!/bin/bash
# Build TI's own CoreMark port (SDK benchmarks/coremark, files unchanged) with printf sent to a RAM buffer, and the iteration
# count raised from TI's 1,000 to 9,000 so a run lasts about 10 s (CoreMark's rule). Only a copy of app_cfg.h differs (ITERATIONS).
#   build_ti_coremark.sh <opt>     -> $OUT/coremark-ti-<opt>-ram/coremark.out
set -u
OPT="${1:-O3}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; TC="$SDK/examples/benchmarks/coremark"; LP="$TC/am13e230x_lp/m33_nortos"
CFG="$OUT/ti-coremark-cfg"; mkdir -p "$CFG"; sed 's/#define ITERATIONS 1000/#define ITERATIONS 9000/' "$LP/app_cfg.h" > "$CFG/app_cfg.h"
grep -q "ITERATIONS 9000" "$CFG/app_cfg.h" || { echo "could not set ITERATIONS"; exit 1; }
EXTRA_SRC="$TC/core_list_join.c $TC/core_main.c $TC/core_matrix.c $TC/core_state.c $TC/core_util.c $HERE/bench/coremark/ti_printf.c" \
  "$HERE/tools/build_one.sh" "coremark-ti-ram" "$OPT" "$TC/core_portme.c" -fno-builtin-printf -I"$CFG" -I"$TC" -I"$SDK/examples/empty/am13e230x_lp"
