#!/bin/bash
# Cross-build between the F3 SDK (LP-EM-CC2340R5) and the Wi-Fi SDK (LP-EM-CC35X1), both ways, for the
# TI Drivers examples both ship under the same name. Build only, nothing flashed. Each twin gets a baseline
# build (the target's own example, untouched) and a swapped build (the source board's app .c files dropped
# into the target's project, no edits). Rung 2 of the ladder (portability/README.md).
#   crossbuild_f3_wifi.sh <work dir> <results jsonl> <direction: f3_to_wifi | wifi_to_f3>
W="$1"; OUT="$2"; DIR=${3:-f3_to_wifi}; mkdir -p "$W/logs"
F3=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
WIFI=/Applications/ti/simplelink_wifi_sdk_10_20_00_39
TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
if [ "$DIR" = f3_to_wifi ]; then
  A=$F3/examples/rtos/LP_EM_CC2340R5/drivers; B=$WIFI/examples/rtos/LP_EM_CC35X1/drivers; FROM=LP_EM_CC2340R5; TO=LP_EM_CC35X1
  MK="SIMPLELINK_WIFI_SDK_INSTALL_DIR=$WIFI SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR=$TB SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER="
else
  A=$WIFI/examples/rtos/LP_EM_CC35X1/drivers; B=$F3/examples/rtos/LP_EM_CC2340R5/drivers; FROM=LP_EM_CC35X1; TO=LP_EM_CC2340R5
  MK="SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$F3 SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER="
fi
for e in $(comm -12 <(ls $A) <(ls $B)); do
  for mode in baseline swapped; do
    d="$W/$DIR/$mode/$e"; rm -rf "$d"; mkdir -p "$(dirname "$d")"; cp -R "$B/$e" "$d"
    [ -d "$d/freertos/ticlang" ] || continue
    if [ $mode = swapped ]; then for c in "$A/$e"/*.c; do cp "$c" "$d/"; done; fi
    log="$W/logs/$DIR-$mode-$e.log"
    ( cd "$d/freertos/ticlang" && make $MK ) >"$log" 2>&1
    rc=$?; o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1}')
    err=$(grep -m1 -E "error:|undefined symbol|Error" "$log" | sed -E 's/^.*error: //' | cut -c1-220 | tr '"\\' "''")
    same=""; [ $mode = swapped ] && same=$(diff -q <(cat "$A/$e"/*.c) <(cat "$B/$e"/*.c) >/dev/null && echo identical || echo differs)
    ok=$([ $rc = 0 ] && [ -n "$o" ] && echo true || echo false)
    echo "{\"src\":\"$FROM:$e\",\"dst\":\"$TO:$e\",\"mode\":\"$mode\",\"ok\":$ok,\"text\":${size:-null},\"app_source\":\"$same\",\"first_error\":\"$err\",\"date\":\"$(date +%F)\"}" >>"$OUT"
    echo "$DIR $mode $e ok=$ok ${size:-} $err"
    rm -rf "$d"/freertos/ticlang/*.obj
  done
done
