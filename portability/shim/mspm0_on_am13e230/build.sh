#!/bin/bash
# Build an MSPM0 driverlib program for the LP-AM13E230 with the shim header, from the terminal.
#   build.sh <gpio|uart|pwm> [extra .c files or -D flags ...] [--flash]
# The program's main.c is the MSPM0 source, unchanged unless the README says otherwise; example.syscfg is the AM13 equivalent
# of the MSPM0 .syscfg. Output: $OUT/mspm0on13-<name>/<name>.out. Same toolchain and flags as am13e230/tools/build_example.sh.
set -u
NAME="$1"; shift
HERE="$(cd "$(dirname "$0")" && pwd)"; BOARD="$HERE/../../../am13e230"
OUT="${OUT:-$PWD/out/am13e230}/mspm0on13-$NAME"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
SRCS=("$HERE/$NAME/main.c"); DEFS=(); FLASH=0
for a in "$@"; do case "$a" in --flash) FLASH=1;; -D*) DEFS+=("$a");; *) SRCS+=("$a");; esac; done
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors --output "$OUT/gen" "$HERE/$NAME/example.syscfg" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; tail -5 "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$HERE -I$HERE/$NAME -I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "${SRCS[@]}" "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall ${DEFS[*]:-} $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed ($f): $OUT/build.log"; grep "error" "$OUT/build.log" | head -20; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" -Wl,--rom_model -Wl,--reread_libs \
  -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/$NAME.map" "-L$CC/lib" -llibc.a -o "$OUT/$NAME.out" ) >> "$OUT/build.log" 2>&1 || { echo "Link failed: $OUT/build.log"; grep -m8 "error\|undefined" "$OUT/build.log"; exit 1; }
echo "built $OUT/$NAME.out: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0) ' "$OUT/$NAME.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
rm -rf "$OUT/obj"
if [ "$FLASH" = 1 ]; then "$DSLITE" flash -c "$BOARD/AM13E23019_AL130001.ccxml" -e -f -v "$OUT/$NAME.out" > "$OUT/flash.log" 2>&1 || { echo "Flash failed"; tail -5 "$OUT/flash.log"; exit 1; }; echo flashed; fi
