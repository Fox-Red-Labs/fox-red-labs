#!/bin/bash
# Dhrystone 2.1 for the LP-AM13E230 (TI Arm Clang, sources unmodified from mspm0l1306/bench/dhrystone/src):
#   build_dhrystone.sh <O3|Os|O3i>      output $OUT/dhry-<opt>/dhry.out (O3 and Os forbid inlining, O3i allows it)
set -e
V=$1; HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}"; RUNS=${RUNS:-3000000}
case $V in O3) OPT="-O3 -fno-inline";; Os) OPT="-Os -fno-inline";; O3i) OPT="-O3";; *) echo "O3|Os|O3i"; exit 1;; esac
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
LINKCMD="$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd"
SRC="$REPO/mspm0l1306/bench/dhrystone/src"; G="$OUT/gen-coremark"; O="$OUT/dhry-$V"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
COMMON="$ARCH $INC -I$SRC -I$HERE/bench/dhrystone -I$G -g -DDH_RUNS=$RUNS -DCPU_HZ=200000000u"
DHRY="$OPT $COMMON -std=gnu89 -w -DTIME -include string.h -Dmain=dhry_main -Dprintf=dh_printf -Dscanf=dh_scanf -Dmalloc=dh_malloc"
for f in dhry_1 dhry_2; do "$CC/bin/tiarmclang" $DHRY -c "$SRC/$f.c" -o $f.o; done
"$CC/bin/tiarmclang" -O2 $COMMON -Wall -DTIME -c "$HERE/bench/dhrystone/dhry_port.c" -o dhry_port.o
"$CC/bin/tiarmclang" -O2 $COMMON -Wall -DTIME -include string.h -c "$HERE/bench/dhrystone/main.c" -o main.o
"$CC/bin/tiarmclang" -O2 $COMMON -c "$G/ti_sdk_dl_config.c" -o ti_sdk_dl_config.o
"$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$LINKCMD" -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,dhry.map "-L$CC/lib" -llibc.a -o dhry.out
echo "built $O/dhry.out"; ls -l dhry.out
