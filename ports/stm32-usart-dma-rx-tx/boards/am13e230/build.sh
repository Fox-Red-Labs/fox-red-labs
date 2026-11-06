#!/bin/bash
# Build the port: SysConfig -> compile main.c, lwrb (unchanged upstream file) and generated code -> link.
#   ./build.sh [baud]     baud is the one build-time define (default 115200)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; BAUD="${1:-115200}"
UP="$HERE/../../common/lwrb/src"; O="$HERE/build"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
S=/Applications/ti/sysconfig_1.28.0
rm -rf "$O"; mkdir -p "$O/gen" "$O/obj"
"$S/nodejs/node" "$S/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors --output "$O/gen" "$HERE/example.syscfg" > "$O/sysconfig.log" 2>&1
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source -I$O/gen -I$UP/include -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$HERE/main.c" "$UP/lwrb/lwrb.c" "$O"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall -DAPP_BAUD=$BAUD ${EXTRA_DEFS:-} $INC -c "$f" -o "$O/obj/$(basename "${f%.c}").o" >> "$O/build.log" 2>&1 || { grep -m8 error "$O/build.log"; exit 1; }
done
( cd "$O/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$O/usart_dma.map" "-L$CC/lib" -llibc.a -o "$O/usart_dma.out" ) >> "$O/build.log" 2>&1 || { grep -m8 "error\|undefined" "$O/build.log"; exit 1; }
echo "built $O/usart_dma.out"; grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|FLASH_BANK1) ' "$O/usart_dma.map"
"$CC/bin/tiarmclang" --version >/dev/null 2>&1; "$CC/bin/tiarmllvm-size" "$O/usart_dma.out" 2>/dev/null || ls "$CC/bin" | grep -i size
