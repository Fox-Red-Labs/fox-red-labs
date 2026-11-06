#!/bin/bash
# Build one of TI's AM13E2x SDK LaunchPad examples from the terminal, the way prebuild.sh does, for the sweep:
#   build_example.sh <group/example> [opt]       e.g. driverlib/uart/uart_external_loopback_polling
# Output: $OUT/ex-<group>-<example>/<example>.out and .map (OUT defaults to out/am13e230).
# Prints the flash and RAM use from the linker map. Needs CCStudio 21.0.1, SysConfig 1.28.0 and the SDK.
set -u
EX="$1"; OPT="${2:-O2}"; NAME="$(basename "$EX")"
OUT="${OUT:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
D="$SDK/examples/$EX"; LP="$D/am13e230x_lp/m33_nortos"
[ -f "$LP/example.syscfg" ] || { echo "no LaunchPad build of $EX"; exit 2; }
O="$OUT/ex-$(echo "$EX" | tr / -)${BAUD:+-$BAUD}"; rm -rf "$O" "$O.log"; mkdir -p "$O/gen" "$O/obj"
SYSCFG="$LP/example.syscfg"
# BAUD=<n>: build with every targetBaudRate in the syscfg replaced (used for the UART speed sweep)
if [ -n "${BAUD:-}" ]; then SYSCFG="$O/example.syscfg"; sed -E "s/(targetBaudRate *= *)[0-9]+/\1$BAUD/" "$LP/example.syscfg" > "$SYSCFG"; fi
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors --output "$O/gen" "$SYSCFG" > "$O.log" 2>&1 || { echo "SysConfig failed: $O.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source/mathlib/fastrts -I$SDK/source/cmsis/DSP/Include -I$SDK/source -I$O/gen -I$D -I$LP -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$D"/*.c "$O"/gen/*.c; do
  "$CC/bin/tiarmclang" -$OPT $ARCH -g -Wall $INC -c "$f" -o "$O/obj/$(basename "${f%.c}").o" >> "$O.log" 2>&1 || { echo "compile failed: $O.log"; exit 1; }; done
( cd "$O/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$O/$NAME.map" "-L$CC/lib" -llibc.a -o "$O/$NAME.out" ) >> "$O.log" 2>&1 || { echo "link failed: $O.log"; exit 1; }
echo "built $O/$NAME.out: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|FLASH_BANK1) ' "$O/$NAME.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
grep -c "BCR_CONFIG" "$O/$NAME.map" | sed 's/^/BCR_CONFIG lines in map: /'
