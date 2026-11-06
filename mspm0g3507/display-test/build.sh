#!/bin/bash
# Build the display test with TI Arm Clang on the SysConfig output in gen/
# (regenerate gen/ from display.syscfg with the SysConfig CLI if pins change).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
build() { # out-name extra-cflags
  local GEN="$HERE/${GEN_DIR:-gen}" O="$HERE/out/$1"; mkdir -p "$O"; cd "$O"
  local CF="-O2 $ARCH @$GEN/device.opt -I$GEN -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall $2"
  "$CC/bin/tiarmclang" $CF -c "$HERE/${3:-display_test}.c" -o main.o
  "$CC/bin/tiarmclang" $CF -c "$GEN/ti_msp_dl_config.c" -o cfg.o
  "$CC/bin/tiarmclang" $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c" -o st.o
  "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 main.o cfg.o st.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" "$GEN/device_linker.cmd" \
     -Wl,-m,"$1.map" -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$1.out"
  rm -f *.o; "$CC/bin/tiarmnm" "$1.out" | grep " res$" || true; "$CC/bin/tiarmsize" "$1.out" | tail -1
}
build display_8mhz -DSPI_SCR=1
build display_16mhz -DSPI_SCR=0
build display_1mhz "-DSPI_SCR=15 -DFULL_INIT"
build display_readback "-DSPI_SCR=15 -DFULL_INIT -DREADBACK"
build display_exp "-DSPI_SCR=15 -DEXPERIMENT"
for f in "$HERE"/private/screens/*.h; do [ -f "$f" ] && build "screen_$(basename "$f" .h)" "-DSPI_SCR=1 -DPHOTO -include $f"; done
build display_diag "-DSPI_SCR=1 -DDIAG"
build probe "" probe
build bltest "" bltest
build bloff "" bloff
build perf "" perf
build perf_fastboot "-DFAST_BOOT" perf
# 80 MHz CPU: SPI1 runs from the 80 MHz clock, so dividers 1 and 0 give 20 and 40 MHz
GEN_DIR=gen80 build perf80 "-DFAST_BOOT -DREAD_SCR=7" perf
