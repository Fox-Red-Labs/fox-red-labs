#!/bin/bash
# Build and run the Mac host version. Output: ports/wasm3/host/out/wasm3-host
set -e
cd "$(dirname "$0")"; bash ../patches/apply.sh ../out-src; W=../out-src/source; mkdir -p out
CORE=""; for f in $W/m3_*.c; do case $f in */m3_core.c|*/m3_api_*) ;; *) CORE="$CORE $f";; esac; done
# m3_api_* (WASI, libc, tracer) are not needed. m3_core.c comes in through common/m3_core_probe.c.
clang -O2 -std=c99 -Wall -Wno-unused-function -Dd_m3FixedHeap=262144 -Dd_m3CodePageAlignSize=1024 -I$W -DFRL_BENCH_N=${BENCH_N:-2000000} -DFRL_FIB_N=${FIB_N:-30} \
  -o out/wasm3-host $CORE ../common/m3_core_probe.c ../common/frl_wasm.c ../common/native.c main.c
./out/wasm3-host
