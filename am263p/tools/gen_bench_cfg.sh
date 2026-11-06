#!/bin/bash
# One-time: copy the SysConfig output of TI's coremark_benchmark example for am263px-lp (made by the SDK makefile) to $OUT/gen.
set -eu
OUT="${OUT:-$PWD/out/am263p}"
D=$HOME/ti/mcu_plus_sdk_am263px_26_01_00_08/examples/benchmarks/coremark_benchmark/am263px-lp/r5fss0-0_nortos/ti-arm-clang
[ -d "$D/generated" ] || { echo "build the SDK example once first (make all with env.sh arguments)"; exit 1; }
mkdir -p "$OUT/gen"; cp "$D"/generated/* "$OUT/gen/"; echo "copied to $OUT/gen"
