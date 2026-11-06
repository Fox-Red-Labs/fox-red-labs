#!/bin/bash
# H.264 decode program for the LP-MSPM33C321A at 160 MHz, TI Arm Clang, code and streams in flash.   build_h264.sh <set> <opt> [extra flags]
#   set: s176 (176x144) | s240 (176x144 and 240x240)        opt: Os | O2 | O3        extra flags e.g. -mcpu=cortex-m33+dsp is the default already (see README)
# Shared decoder, harness, streams and Mac reference: portability/h264/.  Output <dir>/h264-<set>-<opt>/h264.out
set -e
SET=$1; OPT=$2; shift 2; X="$*"; HERE="$(cd "$(dirname "$0")/.." && pwd)"; P="$HERE/../portability/h264"; R="${R:-$PWD/out/mspm33c321a}"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
W="${W:-$PWD/out/h264/work}"; H=$("$P/fetch_src.sh" "$W"); G="$R/gen160"; OUT="$R/h264-$SET-$OPT${TAG:-}"; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
case $SET in
  s176) S="$P/streams/stream-176x144.h264"; ARENA=110000;;
  s240) S="$P/streams/stream-176x144.h264 $P/streams/stream-240x240.h264"; ARENA=231424;;
esac
"$P/gen_streams.sh" streams.c $S
sed "s/--stack_size=8192/--stack_size=${STACK:-8192}/" "$G/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CF="-$OPT $ARCH @$G/device.opt -I$H -I$P/shim -I$P -I$HERE/bench -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -DCPU_HZ=160000000u -DPASSES=3 -DMAX_REC=8 -DARENA_BYTES=$ARENA $X"
for f in "$H"/*.c; do "$CC/bin/tiarmclang" $CF -std=gnu99 -w -Dmalloc=h_malloc -Dfree=h_free -include stdlib.h -c "$f" -o "$(basename "${f%.c}").o"; done
for f in "$P/h264_run.c" "$P/h_alloc.c" streams.c "$HERE/bench/h264/main.c" "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  "$CC/bin/tiarmclang" $CF -std=gnu99 -c "$f" -o "$(basename "${f%.c}").o"; done
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,h264.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o h264.out
echo "built $OUT/h264.out"; "$CC/bin/tiarmobjdump" -h h264.out 2>/dev/null | grep -E "\.text|\.bss|\.rodata|\.const|\.data" || true; ls -l h264.out
