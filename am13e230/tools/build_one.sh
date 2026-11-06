#!/bin/bash
# Build one of our bench programs for the LP-AM13E230 with the prebuild.sh flags: build_one.sh <name> <opt> <main.c> <extra flags...>
# Output: $OUT/<name>-<opt>/<name>.out (+ .map). Uses the gen-coremark SysConfig output (TI's 200 MHz clock). For the sources to build
# alongside main.c, set EXTRA_SRC="a.c b.c".
set -u
NAME="$1"; OPT="$2"; MAIN="$3"; shift 3
HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source/mathlib/fastrts -I$SDK/source/cmsis/DSP/Include -I$SDK/source"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
LINKCMD="${LINKCMD:-$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd}"   # a caller may pass its own copy
G="$OUT/gen-coremark"; O="$OUT/$NAME-$OPT"; rm -rf "$O" "$O.log"; mkdir -p "$O"
F="-$OPT $ARCH -g -Wall $INC -I$G -DCPU_HZ=200000000u $*"
for f in "$MAIN" ${EXTRA_SRC:-} "$G"/*.c; do
  "$CC/bin/tiarmclang" $F -c "$f" -o "$O/$(basename "${f%.c}").o" >> "$O.log" 2>&1 || { echo "compile failed: $O.log"; exit 1; }; done
( cd "$O" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$LINKCMD" -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$NAME.map" "-L$CC/lib" -llibc.a -o "$NAME.out" ) >> "$O.log" 2>&1 || { echo "link failed: $O.log"; exit 1; }
echo "built $O/$NAME.out"
