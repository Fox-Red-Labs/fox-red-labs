#!/bin/bash
# Build SimpleFOC (Arduino-FOC, unmodified, in third-party/simplefoc/) for the LP-AM13E230 + DRV8376EVM. BUILD ONLY: never flashes.
#   ./build.sh        Output: out/simplefoc/simplefoc.out and .map (outside the repo unless OUT is set)
# Toolchain and link recipe as ../../am13e230/bldc-drv8376/build.sh (TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, AM13E2x SDK 26.01.00.03).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
OUT="${OUT:-$PWD/out}/simplefoc"
FOC="$REPO/third-party/simplefoc/arduino-foc/src"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
t0=$(python3 -c 'import time;print(time.time())')
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --output "$OUT/gen" "$HERE/simplefoc.syscfg" > "$OUT/sysconfig.log" 2>&1 \
  || { echo "SysConfig failed: $OUT/sysconfig.log"; cat "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$HERE/shim -I$FOC -I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source -I$OUT/gen"
CXXF="-x c++ -std=gnu++17 -fno-exceptions -fno-rtti -DSIMPLEFOC_DISABLE_DEBUG -DFRL_DRY=${FRL_DRY:-0} -O2 -ffunction-sections -fdata-sections"
CF="-O2 -ffunction-sections -fdata-sections"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
# SimpleFOC files that the Hall + 6PWM + BLDC voltage-mode path needs (nothing else is compiled)
FOCSRC="BLDCMotor.cpp common/base_classes/FOCMotor.cpp common/base_classes/Sensor.cpp common/base_classes/CurrentSense.cpp common/foc_utils.cpp
 common/pid.cpp common/lowpass_filter.cpp common/time_utils.cpp sensors/HallSensor.cpp drivers/BLDCDriver6PWM.cpp"
: > "$OUT/build.log"
for f in $FOCSRC; do o="$OUT/obj/foc_$(echo "$f" | tr '/' '_').o"
  "$CC/bin/tiarmclang" $ARCH $CXXF -g -Wall -Wno-unused-parameter $INC -c "$FOC/$f" -o "$o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $f (see $OUT/build.log)"; grep error "$OUT/build.log" | head -20; exit 1; }
done
for f in "$HERE/hal/am13_hal.cpp" "$HERE/app/main.cpp"; do
  "$CC/bin/tiarmclang" $ARCH $CXXF -g -Wall -Wno-unused-parameter $INC -c "$f" -o "$OUT/obj/app_$(basename "${f%.cpp}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $f (see $OUT/build.log)"; grep error "$OUT/build.log" | head -20; exit 1; }
done
for f in "$OUT"/gen/*.c; do "$CC/bin/tiarmclang" $ARCH $CF -g $INC -c "$f" -o "$OUT/obj/gen_$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $f"; grep error "$OUT/build.log" | head; exit 1; }; done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/simplefoc.map" "-L$CC/lib" -llibc.a -llibc++.a -llibc++abi.a -o "$OUT/simplefoc.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; grep -E "error|undefined" "$OUT/build.log" | head -20; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
echo "built $OUT/simplefoc.out in $(python3 -c "print(f'{$t1-$t0:.1f}')") s; warnings: $(grep -c 'warning:' "$OUT/build.log")"
"$CC/bin/tiarmllvm-size" "$OUT/simplefoc.out" 2>/dev/null || ls "$CC/bin" | grep -i size
