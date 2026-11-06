#!/bin/bash
# MSPM0 DriverLib examples dropped into the LP-MSPM33C321A's twin projects (same example name in both
# SDKs), build only. Baseline: the MSPM33 example untouched. Swapped: the MSPM0 board's .c files copied
# over the MSPM33 project's, no edits; the MSPM33 SysConfig file and linker file stay. This measures the
# "names shared for GPIO, UART, timers" claim of matrix row X7 on real examples. Rung 2 of the ladder.
#   crossbuild_mspm0_mspm33.sh <work dir> <results jsonl> [MSPM0 board] [categories...]
W="$1"; OUT="$2"; FROM=${3:-LP_MSPM0G3519}; shift 3 2>/dev/null; CATS=${@:-driverlib}; mkdir -p "$W/logs"
M0=/Applications/ti/mspm0_sdk_2_11_00_07; M33=/Applications/ti/mspm33_sdk_1_04_00_00
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
for cat in $CATS; do
A=$M0/examples/nortos/$FROM/$cat; B=$M33/examples/nortos/LP_MSPM33C321A/$cat
for e in $(comm -12 <(ls $A) <(ls $B)); do
  for mode in baseline swapped; do
    d="$W/m0m33/$mode/$cat/$e"; rm -rf "$d"; mkdir -p "$(dirname "$d")"; cp -R "$B/$e" "$d"
    [ -d "$d/ticlang" ] || continue
    if [ $mode = swapped ]; then for c in "$A/$e"/*.c; do [ -f "$c" ] && cp "$c" "$d/"; done; fi
    log="$W/logs/m0m33-$mode-$cat-$e.log"
    ( cd "$d/ticlang" && make MSPM33_SDK_INSTALL_DIR=$M33 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER= ) >"$log" 2>&1
    rc=$?; o=$(ls "$d"/ticlang/*.out 2>/dev/null | head -1)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1}')
    err=$(grep -m1 -E "error:|undefined symbol" "$log" | sed -E 's/^.*error: //' | cut -c1-220 | tr '"\\' "''")
    same=""; [ $mode = swapped ] && same=$(diff -q <(cat "$A/$e"/*.c 2>/dev/null) <(cat "$B/$e"/*.c 2>/dev/null) >/dev/null && echo identical || echo differs)
    ok=$([ $rc = 0 ] && [ -n "$o" ] && echo true || echo false)
    echo "{\"src\":\"$FROM:$cat/$e\",\"dst\":\"LP_MSPM33C321A:$cat/$e\",\"mode\":\"$mode\",\"ok\":$ok,\"text\":${size:-null},\"app_source\":\"$same\",\"first_error\":\"$err\",\"date\":\"$(date +%F)\"}" >>"$OUT"
    echo "$mode $cat/$e ok=$ok ${size:-} ${err:0:90}"
    [ -n "${KEEP:-}" ] || rm -rf "$d"
  done
done
done
