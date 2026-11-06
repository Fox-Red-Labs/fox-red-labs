#!/bin/bash
# Bare-metal Dhrystone 2.1 for the LP-MSPM33C321A with TI Arm Clang, sources unmodified (the L1306 build shrank one array for its 4 kB of RAM; here the 256 kB fits it).
#   build_dhrystone.sh <opt: O3|Os|O3i> <mhz: 32|160> [dir]     output <dir>/dhry-<opt>-<mhz>/dhry.out
set -e
V=$1; MHZ=$2; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${3:-$PWD/out/mspm33c321a}"
case $V in O3) OPT="-O3 -fno-inline";; Os) OPT="-Os -fno-inline";; O3i) OPT="-O3";; *) echo "O3|Os|O3i"; exit 1;; esac
case $MHZ in 32) RUNS=${RUNS:-300000};; 160) RUNS=${RUNS:-1500000};; *) echo "32|160"; exit 1;; esac
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SRC="$HERE/../mspm0l1306/bench/dhrystone/src"; G="$R/gen$MHZ"; OUT="$R/dhry-$V-$MHZ"; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
sed "s/--stack_size=256/--stack_size=2048/" "$G/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
COMMON="$ARCH @$G/device.opt -I$SRC -I$HERE/bench/dhrystone -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -DDH_RUNS=$RUNS -DCPU_HZ=${MHZ}000000u"
DHRY="$OPT $COMMON -std=gnu89 -w -DTIME -include string.h -Dmain=dhry_main -Dprintf=dh_printf -Dscanf=dh_scanf -Dmalloc=dh_malloc"
for f in dhry_1 dhry_2; do "$CC/bin/tiarmclang" $DHRY -c "$SRC/$f.c" -o $f.o; done
"$CC/bin/tiarmclang" -O2 $COMMON -Wall -DTIME -c "$HERE/bench/dhrystone/dhry_port.c" -o dhry_port.o
"$CC/bin/tiarmclang" -O2 $COMMON -Wall -DTIME -include string.h -c "$HERE/bench/dhrystone/main.c" -o main.o
"$CC/bin/tiarmclang" -O2 $COMMON -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC/bin/tiarmclang" -O2 $COMMON -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,dhry.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o dhry.out
