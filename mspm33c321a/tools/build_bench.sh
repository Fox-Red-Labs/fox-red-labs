#!/bin/bash
# Build bench/<name>/main.c (plus extra C files given in EXTRA) for the LP-MSPM33C321A at -O<opt> and 32 or 160 MHz.
#   build_bench.sh <name> <opt: O2|O3|Os> <mhz> [dir]    output <dir>/<name>-<opt>-<mhz>/bench.out  (RAM variables read with run_image.py)
set -e
N=$1; OPT=$2; MHZ=$3; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${4:-$PWD/out/mspm33c321a}"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
G="${GEN:-$R/gen$MHZ}"; O="$R/$N-$OPT-$MHZ"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
sed "s/--stack_size=256/--stack_size=${STACK:-4096}/" "$G/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$HERE/bench/$N -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source $EXTRAINC -gdwarf-3 -Wall -DCPU_HZ=${MHZ}000000u $EXTRAFLAGS"
for f in "$HERE/bench/$N/main.c" $EXTRA "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  "$CC/bin/tiarmclang" $CF -c "$f" -o "$(basename "${f%.c}").o"; done
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" $EXTRALIBS device_linker.cmd \
  -Wl,-m,bench.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o bench.out
grep -E '^ +(BCR|BSL)_CONFIG ' bench.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}'
