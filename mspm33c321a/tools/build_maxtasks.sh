#!/bin/bash
# Build bench/rtos/maxtasks.c against a FreeRTOS kernel built from TI's source with a bigger heap.
#   build_maxtasks.sh <heap bytes>    output <run>/maxtasks-<heap>/freertos/ticlang/blink_led.out (prints LINK FAIL if it does not fit)
set -e
HEAP=${1:?heap bytes}; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/mspm33c321a}"; O="$R/maxtasks-$HEAP"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
KB="$R/maxtasks-kernel-$HEAP"
rm -rf "$O" "$KB"; mkdir -p "$KB"
cp "$SDK/kernel/freertos/builds/LP_MSPM33C321A/release/FreeRTOSConfig.h" "$KB/"; cp -R "$SDK/kernel/freertos/builds/LP_MSPM33C321A/release/ticlang" "$KB/ticlang"
rm -f "$KB"/ticlang/*.obj "$KB"/ticlang/freertos.lib "$KB"/ticlang/*.projectspec
sed -i "" "s/configTOTAL_HEAP_SIZE ((size_t)(8 \* 1024))/configTOTAL_HEAP_SIZE ((size_t)($HEAP))/" "$KB/FreeRTOSConfig.h"
grep -q "($HEAP)" "$KB/FreeRTOSConfig.h"
make -C "$KB/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" TICLANG_ARMCOMPILER="$CC" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh > "$KB/kernel.log" 2>&1 || { tail -5 "$KB/kernel.log"; exit 1; }
cp -R "$SDK/examples/rtos/LP_MSPM33C321A/kernel/blink_led" "$O"
cp "$HERE/bench/rtos/maxtasks.c" "$O/main.c"; cp "$HERE/bench/m33_timer.h" "$O/"
sed -i "" -e "s#-T\./mspm33c321a.cmd#./mspm33c321a.cmd#" "$O/freertos/ticlang/makefile"
make -C "$O/freertos/ticlang" KERNEL_BUILD="$KB" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "maxtasks $HEAP built" || { echo "LINK FAIL $HEAP"; grep -i "error" "$O/build.log" | head -3; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/freertos/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
rm -rf "$KB"
