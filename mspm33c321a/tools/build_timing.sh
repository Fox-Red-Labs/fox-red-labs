#!/bin/bash
# Build the bare-metal GPIO toggle and interrupt latency test (bench/timing) for 32 or 160 MHz at -O2/-O3.
#   build_timing.sh <opt> <mhz> [prebuild-dir]       output: <prebuild-dir>/timing-<opt>-<mhz>/timing.out
HERE="$(cd "$(dirname "$0")/.." && pwd)"; OPT=$1; MHZ=$2; R="${3:-$PWD/out/mspm33c321a}"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
G="$R/gen$MHZ"; O="$R/timing-$OPT-$MHZ"; rm -rf "$O"; mkdir -p "$O"
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -DCPU_HZ=${MHZ}000000u"
cd "$O" || exit 1
for f in "$HERE/bench/timing/main.c" "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  "$CC/bin/tiarmclang" $CF -c "$f" -o "$(basename "${f%.c}").o" || exit 1; done
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" "$G/device_linker.cmd" \
  -Wl,-m,timing.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o timing.out
