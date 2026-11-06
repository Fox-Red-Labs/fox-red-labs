#!/bin/bash
# Build our CoreMark (unmodified EEMBC files + am13e230/bench/coremark) at a lower clock: build_lowclk.sh <div 2|4> <opt O3>
# div 2 = 100 MHz with 1 flash wait state, div 4 = 50 MHz with 1 wait state. TI's 200 MHz SysConfig setup runs first.
# Output: $OUT/coremark-<opt>-<mhz>mhz/coremark.out. Same flags as prebuild.sh.
set -u
DIV="${1:-2}"; OPT="${2:-O3}"; MHZ=$((200/DIV)); WS=1
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
LINKCMD="$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd"
CM="$REPO/third-party/coremark"; P="$HERE/bench/coremark"; G="$OUT/gen-coremark"; O="$OUT/coremark-$OPT-${MHZ}mhz"
rm -rf "$O"; mkdir -p "$O"
{ echo "#define COMPILER_FLAGS \"-$OPT $ARCH, bare metal, $MHZ MHz, $WS flash wait state\""; echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\""; } > "$O.flags.h"
F="-$OPT $ARCH -g -Wall $INC -I$G -I$CM -I$P -DCPU_HZ=${MHZ}000000u -DLOW_CLK_DIV=$DIV -DLOW_CLK_WS=$WS -DITERATIONS=9000 -DPERFORMANCE_RUN=1 -DBOARD_HEADER=\"ti_sdk_dl_config.h\" -include $O.flags.h"
for f in "$CM/core_list_join.c" "$CM/core_matrix.c" "$CM/core_state.c" "$CM/core_util.c" "$P/core_portme.c" "$P/main.c" "$G"/*.c; do
  "$CC/bin/tiarmclang" $F -c "$f" -o "$O/$(basename "${f%.c}").o" >> "$O.log" 2>&1 || { echo "compile failed: $O.log"; exit 1; }; done
"$CC/bin/tiarmclang" $F -Dmain=coremark_main -c "$CM/core_main.c" -o "$O/core_main.o" >> "$O.log" 2>&1 || { echo "compile failed: $O.log"; exit 1; }
( cd "$O" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$LINKCMD" -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"coremark.map" "-L$CC/lib" -llibc.a -o coremark.out ) >> "$O.log" 2>&1 || { echo "link failed: $O.log"; exit 1; }
echo "built $O/coremark.out"
