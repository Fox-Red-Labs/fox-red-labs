#!/bin/bash
# Build and flash the microstepping demo (variant tmu, sw or noisr as $1) for the LP-AM13E230 from the terminal (same toolchain and flags as ../blink.sh).
#   ./build.sh            build only        ./build.sh --flash [probe-serial]   build, then flash over the onboard XDS110
# Output: out/am13e230/stepper-uln2003/microstep/microstep-${V}.out (outside the repo).
set -u
V="${1:-tmu}"; shift || true
case $V in tmu) DEF="-DUSE_TMU";; sw) DEF="";; noisr) DEF="-DUSE_TMU -DNO_ISR";; *) echo "variant tmu|sw|noisr"; exit 2;; esac
HERE="$(cd "$(dirname "$0")" && pwd)"; BOARD="$(cd "$HERE/../.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}/stepper-uln2003/microstep-$V"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
t0=$(python3 -c 'import time;print(time.time())')
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors \
  --output "$OUT/gen" "$HERE/microstep.syscfg" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; cat "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/mathlib/fastrts -I$SDK/source/cmsis/DSP/Include -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source \
 -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$HERE/main.c" "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall $DEF -DCPU_HZ=200000000u $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $OUT/build.log"; cat "$OUT/build.log"; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/microstep-${V}.map" "-L$CC/lib" -llibc.a -o "$OUT/microstep-${V}.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; cat "$OUT/build.log"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
echo "built $OUT/microstep-${V}.out in $(python3 -c "print(f'{$t1-$t0:.1f}')") s: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0) ' "$OUT/microstep-${V}.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
if [ "${1:-}" = "--flash" ]; then
  SER="${2:-$(ls "$BOARD"/AM13E23019_*.ccxml | head -1 | sed 's/.*_\(.*\)\.ccxml/\1/')}"
  "$DSLITE" flash -c "$BOARD/AM13E23019_$SER.ccxml" -e -f -v "$OUT/microstep-${V}.out" > "$OUT/flash.log" 2>&1 || { echo "Flash failed: $OUT/flash.log"; tail -5 "$OUT/flash.log"; exit 1; }
  t2=$(python3 -c 'import time;print(time.time())')
  echo "flashed in $(python3 -c "print(f'{$t2-$t1:.1f}')") s; the stepper program is running (LED4 on while it steps)"
fi
