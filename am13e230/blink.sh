#!/bin/bash
# One-command blink for the LP-AM13E230: TI's driverlib/gpio/gpio_toggle_output (LED4, red, PA5)
# with our own blink period, built from the terminal and flashed over the onboard XDS110.
#
#   ./blink.sh [ms] [probe-serial]      default 250 ms (TI's example uses 500), probe from the one board on USB
#
# Builds into $OUT/blink-<ms>/ ($OUT defaults to ./build-am13e230 in the folder you run it from) with TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0
# and the SDK's prebuilt libraries (the same flags as prebuild.sh), then flashes with UniFlash's DSLite and
# prints the build and flash seconds. Needs: CCStudio 21.0.1, the AM13E2x SDK 26.01.00.03.STS, UniFlash 9.6.0.
set -u
MS="${1:-250}"; SER="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-$PWD/build-am13e230}/blink-$MS"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
EX="$SDK/examples/driverlib/gpio/gpio_toggle_output"
if [ -z "$SER" ]; then SER=$(ls "$HERE"/AM13E23019_*.ccxml 2>/dev/null | head -1 | sed 's/.*_\(.*\)\.ccxml/\1/'); fi
CCXML="$HERE/AM13E23019_$SER.ccxml"
[ -f "$CCXML" ] || { echo "No $CCXML. Run tools/identify.sh <probe-serial> first."; exit 2; }

rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
t0=$(python3 -c 'import time;print(time.time())')
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors \
  --output "$OUT/gen" "$EX/am13e230x_lp/m33_nortos/example.syscfg" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; exit 1; }
sed "s/DEVICE_DELAY_MS(500)/DEVICE_DELAY_MS($MS)/" "$EX/main.c" > "$OUT/main.c"
grep -q "DEVICE_DELAY_MS($MS)" "$OUT/main.c" || { echo "Could not change the delay in main.c"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source \
 -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$OUT/main.c" "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $OUT/build.log"; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/blink.map" "-L$CC/lib" -llibc.a -o "$OUT/blink.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
"$DSLITE" flash -c "$CCXML" -e -f -v "$OUT/blink.out" > "$OUT/flash.log" 2>&1 || { echo "Flash failed: $OUT/flash.log"; tail -5 "$OUT/flash.log"; exit 1; }
t2=$(python3 -c 'import time;print(time.time())')
python3 -c "print(f'build {$t1-$t0:.1f} s, flash {$t2-$t1:.1f} s, LED4 period now ${MS} ms')"
