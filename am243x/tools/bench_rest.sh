#!/bin/bash
# The rest of the core suite on R5F core 0 of the LP-AM243, from the sources the AM263P used: Dhrystone, sin and cos, divide, the FOC step, GPIO write cost and interrupt latency.
# bench_rest.sh [log folder]. Three runs each, one console log per run. Sources stay in am263p/bench; only the build differs.
cd "$(dirname "$0")/../.." || exit 1
LOGS="${1:-am243x/bench/logs}"; mkdir -p "$LOGS"
export OUT="${OUT:-$PWD/out/am243x}"
B=am243x/tools/build_bench.sh; D="$PWD/mspm0l1306/bench/dhrystone/src"; A=am263p/bench
run() { # label name opt-mem-dir endtext
  for r in 1 2 3; do
    L="$LOGS/$1-r$r.txt"
    ~/.venvs/csi/bin/python am243x/tools/ram_capture.py "$OUT/$3/$2.out" "$L" --secs 90 --until "$4" > /dev/null
    echo "$1 r$r $(grep -a -E '^(DHRY runs|MATH sincos|FOC best|TIMING )' "$L" | tr '\n' ' ' | cut -c1-220)"
  done
}
one() { # label name opt mainc endtext extra-src flags...
  label=$1; name=$2; opt=$3; main=$4; end=$5; EXTRA_SRC="$6"; shift 6
  EXTRA_SRC="$EXTRA_SRC" "$B" "$name" "$opt" ocram "$main" "$@" > /dev/null || { echo "BUILD FAIL $label"; return; }
  run "$label" "$name" "$name-$opt-ocram" "$end"
}
DS="$D/dhry_1.c $D/dhry_2.c $PWD/$A/dhrystone/dhry_port.c"
one dhry-o3-noinline dhry_o3ni o3 $A/dhrystone/main.c "DHRY END" "$DS" -DTIME -DDH_RUNS=5000000u -fno-inline "-I$D"
one dhry-os-noinline dhry_osni os $A/dhrystone/main.c "DHRY END" "$DS" -DTIME -DDH_RUNS=5000000u -fno-inline "-I$D"
one dhry-o3-inline dhry_o3in o3 $A/dhrystone/main.c "DHRY END" "$DS" -DTIME -DDH_RUNS=5000000u "-I$D"
one math-sw math_sw o2 $A/math/main.c "MATH END" ""
one math-sdk math_sdk o2 $A/math/main.c "MATH END" "" -DUSE_SDK
export EXTRA_INC="-I$PWD/c2000-launchpad/bench/ctrl"
one foc-sw foc_sw o2 $A/foc/main.c "FOC END" ""
one foc-sdk foc_sdk o2 $A/foc/main.c "FOC END" "" -DUSE_SDK
unset EXTRA_INC
one timing timing o3 $A/timing/main.c "TIMING END" "" -DGPIO_BASE=0x600000u -DPIN=22u
echo REST_DONE
