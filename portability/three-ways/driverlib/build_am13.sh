#!/bin/bash
# Three ways, AM13E230: clean build for LP-AM13E230. Usage: ./build_am13.sh
# SysConfig on am13e230.syscfg (AM13E2x SDK product), compiles common/app.c + glue_am13.c + the generated file, links with the
# SDK's prebuilt libraries and linker file (same flags as am13e230/blink.sh). Output: build-am13e230/three_ways.out (git-ignored).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
O="$HERE/build-am13e230"; LOG="$HERE/../logs/driverlib-am13e230.log"
rm -rf "$O"; mkdir -p "$O/gen" "$O/obj"
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$HERE/../common -I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source -I$O/gen -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
t0=$(python3 -c 'import time;print(time.time())')
{
  echo "== sysconfig"; "$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --output "$O/gen" "$HERE/am13e230.syscfg"
  for f in "$HERE/../common/app.c" "$HERE/glue_am13.c" "$O"/gen/*.c; do
    echo "== cc $(basename "$f")"; "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall $INC -c "$f" -o "$O/obj/$(basename "${f%.c}").o"
  done
  echo "== link"
  ( cd "$O/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" -Wl,--rom_model -Wl,--reread_libs \
    -Wl,--diag_suppress=10063 -Wl,-m,"$O/three_ways.map" "-L$CC/lib" -llibc.a -o "$O/three_ways.out" )
} > "$LOG" 2>&1 || { tail -20 "$LOG"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
secs=$(python3 -c "print(round($t1-$t0,1))")
cp "$O/three_ways.map" "$HERE/../maps/driverlib-am13e230.map"
echo "== clean build $secs s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CC/bin/tiarmsize" "$O/three_ways.out" | tee -a "$LOG"
grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|FLASH_BANK1) ' "$O/three_ways.map" | tee -a "$LOG"
rm -rf "$O/obj"
