#!/bin/bash
# Build bench/<name>/main.c (plus extra C files in EXTRA, extra flags in EXTRAFLAGS/EXTRAINC/EXTRALIBS) for the LP-AM13E230 at 200 MHz.
#   build_bench.sh <name> <opt: O2|O3|Os> [dir]    output <dir>/<name>-<opt>/bench.out  (RAM variables read with run_image.py)
# Clock and drivers come from TI CoreMark's SysConfig output (gen-coremark, 200 MHz), as in build_dhrystone.sh. STACK sets the stack size.
set -e
N=$1; OPT=$2; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${3:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
G="${GEN:-$R/gen-coremark}"; O="$R/$N-$OPT"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
sed "s/--stack_size=0x1000/--stack_size=${STACK:-0x1000}/" "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" > linker.cmd
if [ -n "${TBLOG:-}" ]; then python3 - <<'P'
s=open("linker.cmd").read(); s=s.replace("    .binit ","    .tblog        : > RAM_C\n    .binit ",1); open("linker.cmd","w").write(s)
P
fi
CF="-$OPT $ARCH $INC -I$G -I$HERE/bench/$N -gdwarf-3 -Wall -DCPU_HZ=200000000u $EXTRAINC $EXTRAFLAGS"
for f in "$HERE/bench/$N/main.c" $EXTRA "$G/ti_sdk_dl_config.c"; do
  "$CC/bin/tiarmclang" $CF -c "$f" -o "$(basename "${f%.c}").o"; done
"$CC/bin/tiarmclang" $ARCH ./*.o $LIBS $EXTRALIBS linker.cmd -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 \
  -Wl,-m,bench.map "-L$CC/lib" -llibc.a -o bench.out
grep -E '^ +(BCR|BSL)_CONFIG ' bench.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}' || true
rm -f ./*.o
echo "built $O/bench.out"
