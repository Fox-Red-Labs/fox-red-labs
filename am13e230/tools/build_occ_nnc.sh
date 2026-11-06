#!/bin/bash
# Build one NNC occupancy image for the LP-AM13E230 from the compile job's output (nnc_am13e230_{npu,soft,cde}/mod.a).
#   build_occ_nnc.sh <model> <npu|soft|cde> [nnc dir]       e.g. build_occ_nnc.sh occ_cnn_8k npu
# Output: $OUT/occnnc-<model>-<variant>/nnc.out and nnc.map (OUT defaults to out/am13e230).
# MAIN=<harness .c> builds another harness against the same NNC library (bench/resnet-nnc/main.c for ResNet-8);
# LINKER=<.cmd> replaces the SDK linker file (bench/resnet-nnc/run.sh notes the one ResNet-8 needs).
# Needs the SysConfig output gen-coremark that prebuild.sh makes (200 MHz, the clock of every AM13E230 row), TI Arm Clang 5.1.1.LTS and the SDK.
# The model's NNC output is read from mspm0g3507/mcu-bench/edgeai/uci-room-occupancy/<model>/nnc_am13e230_<variant>/.
set -eu
M=$1; V=$2
HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$HERE/.."
OUT="${OUT:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
MD="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"; N="${3:-$MD/$M/nnc_am13e230_$V}"
G="$OUT/gen-coremark"; O="$OUT/occnnc-$M-$V"; rm -rf "$O" "$O.log"; mkdir -p "$O"
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source/mathlib/fastrts -I$SDK/source/cmsis/DSP/Include -I$SDK/source -I$G -I$N -I$MD/$M"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibai_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
CF="-O2 $ARCH -g -Wall $INC -DCPU_HZ=200000000u -DMODEL=$M ${EXTRA:-} -DBOARD_HEADER=\"ti_sdk_dl_config.h\""
for f in "${MAIN:-$HERE/bench/occupancy-nnc/main.c}" "$G"/*.c; do "$CC/bin/tiarmclang" $CF -c "$f" -o "$O/$(basename "${f%.c}").o" >> "$O.log" 2>&1; done
( cd "$O" && "$CC/bin/tiarmclang" $ARCH ./*.o "$N/mod.a" $LIBS "${LINKER:-$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd}" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,nnc.map "-L$CC/lib" -llibc.a -o nnc.out ) >> "$O.log" 2>&1
echo "built $O/nnc.out: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|FLASH_BANK1) ' "$O/nnc.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
