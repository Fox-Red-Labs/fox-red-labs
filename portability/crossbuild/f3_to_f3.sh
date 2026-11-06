#!/bin/bash
# Cross-build inside one SDK: every TI Drivers example app from one F3 board, built in the same
# example's project for another F3 board. Usage: f3_to_f3.sh <work dir> <results jsonl> [from board] [to board]
W="$1"; OUT="$2"; FROM=${3:-LP_EM_CC2340R5}; TO=${4:-LP_EM_CC2755P10}; mkdir -p "$W/logs"
F3=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
A=$F3/examples/rtos/$FROM/drivers; B=$F3/examples/rtos/$TO/drivers
for e in $(comm -12 <(ls $A) <(ls $B)); do
  for mode in baseline swapped; do
    d="$W/$mode/$e"; rm -rf "$d"; mkdir -p "$W/$mode"; cp -R "$B/$e" "$d"
    [ -d "$d/freertos/ticlang" ] || continue
    if [ $mode = swapped ]; then for c in "$A/$e"/*.c; do cp "$c" "$d/"; done; fi
    log="$W/logs/$mode-$e.log"
    ( cd "$d/freertos/ticlang" && make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$F3 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT ) >"$log" 2>&1
    rc=$?; o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1}')
    err=$(grep -m1 -E "error:|undefined symbol" "$log" | sed -E 's/^.*error: //' | cut -c1-200 | tr '"\\' "''")
    same=""; [ $mode = swapped ] && same=$(diff -q <(cat "$A/$e"/*.c) <(cat "$B/$e"/*.c) >/dev/null && echo identical || echo differs)
    echo "{\"src\":\"$FROM:$e\",\"dst\":\"$TO:$e\",\"mode\":\"$mode\",\"ok\":$([ $rc = 0 ] && [ -n "$o" ] && echo true || echo false),\"text\":${size:-null},\"app_source\":\"$same\",\"first_error\":\"$err\"}" >>"$OUT"
  done
done
