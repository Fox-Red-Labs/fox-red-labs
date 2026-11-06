#!/bin/bash
# Build bench/rtos/main.c with TI's prebuilt FreeRTOS for the LP-AM13E230 (200 MHz, SysConfig output of the SDK's task_switch example).
#   build_rtos.sh [O2|O3]    output <run>/rtos-<opt>/bench.out
set -e
OPT=${1:-O2}; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
NODE=/Applications/ti/sysconfig_1.28.0/nodejs/node; X=$SDK/examples/kernel/freertos/task_switch/am13e230x_lp/m33_freertos
G="$R/gen-rtos"; if [ ! -d "$G" ]; then mkdir -p "$G"; "$NODE" /Applications/ti/sysconfig_1.28.0/dist/cli.js --product "$SDK/.metadata/product.json" --quiet --output "$G" "$X/example.syscfg" > "$R/gen-rtos.log" 2>&1; fi
O="$R/rtos-$OPT"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include -I$SDK/source/driverlib/am13e230x -I$SDK/source/kernel/freertos/FreeRTOS_Kernel/include -I$SDK/ti_sdk_config/am13e230x/default/freertos_cfg/m33 -I$SDK/source/kernel/freertos/FreeRTOS_Kernel/portable/GCC/ARM_CM33_NTZ/non_secure -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$X -I$X/.. -I$G"
CF="-$OPT $ARCH $INC -gdwarf-3 -Wall -DCPU_HZ=200000000u"
for f in "$HERE/bench/rtos/main.c" "$G"/*.c; do "$CC/bin/tiarmclang" $CF -c "$f" -o "$(basename "${f%.c}").o"; done
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibfreertos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_freertos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
LK=$(ls "$G"/linker*.cmd 2>/dev/null | head -1); [ -n "$LK" ] || LK="$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd"
"$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$LK" -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,bench.map "-L$CC/lib" -llibc.a -o bench.out
grep -E '^ +(BCR|BSL)_CONFIG ' bench.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}' || true
rm -f ./*.o; echo "built $O/bench.out"
