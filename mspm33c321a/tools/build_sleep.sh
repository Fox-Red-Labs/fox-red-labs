#!/bin/bash
# Sleep-current image for the LP-MSPM33C321A. Build only.
#   build_sleep.sh SLEEP|STOP|STANDBY0|SHUTDOWN    output <run>/sleep-<MODE>/bench.out  (32 MHz SYSOSC from gen32)
set -e
M=$1; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUN:-$PWD/out/speed-run/mspm33c321a}"; G="$R/gen32"; O="$R/sleep-$M"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
rm -rf "$O"; mkdir -p "$O"; cd "$O"
CF="-O2 $ARCH @$G/device.opt -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -DMODE_$M $EXTRA"
for f in "$HERE/bench/power/sleep.c" "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  "$CC/bin/tiarmclang" $CF -c "$f" -o "$(basename "${f%.c}").o"; done
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" "$G/device_linker.cmd" -Wl,-m,bench.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o bench.out
grep -E '^ +(BCR|BSL)_CONFIG ' bench.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}' || true
echo "built $O/bench.out"
