#!/bin/bash
# Build the bare-metal timing test for the LP-MSPM0L1306 with TI Arm Clang.  ./build.sh <opt>   (O2, Os, O3)
set -e
OPT=$1; HERE="$(cd "$(dirname "$0")" && pwd)"
. "$(dirname "$0")/../../../tools/frl/toolpaths.sh"
GEN="$HERE/../coremark/syscfg32"; OUT="$HERE/build-$OPT"; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
CFLAGS="-$OPT $ARCH @$GEN/device.opt -I$GEN -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall"
"$CC/bin/tiarmclang" $CFLAGS -c "$HERE/main.c" -o main.o
"$CC/bin/tiarmclang" $CFLAGS -c "$GEN/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC/bin/tiarmclang" $CFLAGS -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0l130x_ticlang.c" -o startup.o
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" "$GEN/device_linker.cmd" \
  -Wl,-m,timing.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o timing.out
