#!/bin/bash
# Host tests for the three patches, each built twice (patched copy, unpatched copy) with AddressSanitizer and UBSan.
#   call_ref recursion (patch 0001), memory.grow wrap (0002), constant-expression opcodes (0003 adds an empty memory header;
#   v0.9.0 already rejects the opcodes, so that test passes both ways).
# Usage: bash tests/run_security_tests.sh     (needs clang; run from anywhere)
cd "$(dirname "$0")/.."; mkdir -p host/out
SAN="-fsanitize=address,undefined -fno-sanitize=function -fno-omit-frame-pointer"
for v in patched unpatched; do
  if [ $v = unpatched ]; then export WASM3_UNPATCHED=1; else unset WASM3_UNPATCHED; fi
  bash patches/apply.sh host/out/src-$v
  W=host/out/src-$v/source; SRC=""; for f in $W/m3_*.c; do case $f in */m3_core.c|*/m3_api_*) ;; *) SRC="$SRC $f";; esac; done
  b() { clang -O1 -g -std=c99 -w $SAN -I$W "$@" $SRC $W/m3_core.c -o host/out/$OUTN-$v || exit 2; }
  OUTN=callref;  b -Dd_m3HasTypedRefs=1 -Dd_m3MaxNativeStack=1048576 tests/callref_test.c
  OUTN=memgrow;  b -Dd_m3MaxLinearMemoryPages=0 tests/memgrow_test.c
  OUTN=initexpr; b tests/initexpr_test.c
  echo "== $v"
  for t in "callref tests/callref_recurse.wasm" "memgrow tests/memgrow_wrap.wasm" "initexpr tests/initexpr_load.wasm"; do
    set -- $t; echo "-- $1"; ASAN_OPTIONS=detect_stack_use_after_return=0 ./host/out/$1-$v $2 2>&1 | grep -v "^    #\|^$" | head -4; echo "   exit code ${PIPESTATUS[0]}"
  done
done

# Ten passes of the app loop: heap_used must be flat (the fix is common/m3_core_probe.c + cleanup() in common/frl_wasm.c)
W=host/out/src-patched/source; SRC=""; for f in $W/m3_*.c; do case $f in */m3_core.c|*/m3_api_*) ;; *) SRC="$SRC $f";; esac; done
clang -O1 -g -std=c99 -w $SAN -fno-sanitize=function -I$W -Dd_m3FixedHeap=65536 -Dd_m3CodePageAlignSize=1024 -DFRL_FIB_N=15 -DFRL_BENCH_N=500 $SRC common/m3_core_probe.c common/frl_wasm.c common/native.c tests/loop_test.c -o host/out/loop && echo "== 10-pass loop" && ./host/out/loop | tail -4
