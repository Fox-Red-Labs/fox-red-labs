#!/bin/bash
# Build one program against TI's STM32G4 HAL compatibility layer for the LP-AM13E230, from the terminal.
#   build.sh <name> <syscfg> <source-dir-or-file> [more sources or -D flags ...] [--flash]
# Compiles every .c in a source dir (or the listed files), the SysConfig output, and links TI's prebuilt
# libstm32g4_hal (source/compatibility/stm32g4_hal). Output: $OUT/stm32g4-<name>/<name>.out and .map.
# Same toolchain and flags as ../tools/build_example.sh.
set -u
NAME="$1"; SYSCFG="$2"; shift 2
OUT="${OUT:-$PWD/out/am13e230}/stm32g4-$NAME"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
HERE="$(cd "$(dirname "$0")" && pwd)"; BOARD="$(cd "$HERE/.." && pwd)"
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
SRCS=(); DEFS=(); INCS=(); FLASH=0
for a in "$@"; do
  case "$a" in
    --flash) FLASH=1 ;;
    -D*) DEFS+=("$a") ;;
    -I*) INCS+=("$a") ;;
    *) if [ -d "$a" ]; then for f in "$a"/*.c; do SRCS+=("$f"); done; else SRCS+=("$a"); fi ;;
  esac
done
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors \
  --output "$OUT/gen" "$SYSCFG" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; tail -5 "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
H=$SDK/source/compatibility/stm32g4_hal
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source/hal/am13e230x/include -I$SDK/ti_sdk_config/am13e230x/default/Hal_Cfg -I$H/STM32G4xx_HAL_Driver/Inc -I$SDK/source -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp ${INCS[*]:-}"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibstm32g4_hal_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibhal_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "${SRCS[@]}" "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall ${DEFS[*]:-} $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed ($f): $OUT/build.log"; grep -m5 "error" "$OUT/build.log"; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/$NAME.map" "-L$CC/lib" -llibc.a -o "$OUT/$NAME.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; grep -m8 "error\|undefined" "$OUT/build.log"; exit 1; }
echo "built $OUT/$NAME.out: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0) ' "$OUT/$NAME.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
rm -rf "$OUT/obj"
if [ "$FLASH" = 1 ]; then
  CCXML="$BOARD/AM13E23019_AL130001.ccxml"
  "$DSLITE" flash -c "$CCXML" -e -f -v "$OUT/$NAME.out" > "$OUT/flash.log" 2>&1 || { echo "Flash failed: $OUT/flash.log"; tail -5 "$OUT/flash.log"; exit 1; }
  echo "flashed $NAME"
fi
