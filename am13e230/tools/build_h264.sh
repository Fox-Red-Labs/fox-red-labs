#!/bin/bash
# H.264 decode program for the LP-AM13E230 at 200 MHz, TI Arm Clang, code and streams in flash, decoder memory in the 96 kB data RAM plus the 32 kB code-optimized RAM (RAM_C).
#   SET=s176|ram96 build_h264.sh <opt: Os|O2|O3> [extra flags]   s176: 176x144, second pool in RAM_C; ram96: 128x96 and 160x112 in the 96 kB data RAM only. Output $OUT/h264-<set>-<opt>/h264.out
# Shared decoder, harness, streams and Mac reference: portability/h264/.
set -e
OPT=$1; SET=${SET:-s176}; shift; X="$*"; HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$HERE/.."; P="$REPO/portability/h264"
OUT0="${OUT:-$PWD/out/am13e230}"; G="$OUT0/gen-coremark"; O="$OUT0/h264-$SET-$OPT${TAG:-}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
TP="python3 $REPO/tools/frl/toolpath.py"; SDK="$($TP am13_sdk --path)" || exit 2; CC="$($TP ticlang --version 5.1.1.LTS --path)" || exit 2   # FRL_AM13_SDK and FRL_TICLANG name other places
W="${W:-$PWD/out/h264/work}"; H=$("$P/fetch_src.sh" "$W")
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
# linker file: stack 6.5 kB (the decoder uses 5.7 kB, measured), no heap, and a section .arena2 in RAM_C
sed -e "s/--stack_size=0x1000/--stack_size=${STACK:-0x1A00}/" -e 's/--heap_size=0x1000/--heap_size=0x100/' -e 's|^    .data         : > RAM_S|    .arena2       : > RAM_C, type = NOLOAD\n    .data         : > RAM_S|' "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" > linker.cmd
grep -q '.arena2' linker.cmd
case $SET in
  s176) STREAMS="$P/streams/stream-176x144.h264"; A2="-DARENA2_BYTES=${ARENA2:-32000}"; ARENA=${ARENA:-80000};;
  ram96) STREAMS="$P/streams/stream-128x96.h264 $P/streams/stream-160x112.h264"; A2=""; ARENA=${ARENA:-84000};;
esac
"$P/gen_streams.sh" streams.c $STREAMS
CF="-$OPT $ARCH $INC -I$H -I$P/shim -I$P -I$HERE/bench -I$G -g -DCPU_HZ=200000000u -DPASSES=3 -DMAX_REC=8 -DARENA_BYTES=$ARENA $A2 -DSTRIP_BYTES=${STRIP:-2816} $X"
for f in "$H"/*.c; do "$CC/bin/tiarmclang" $CF -std=gnu99 -w -Dmalloc=h_malloc -Dfree=h_free -include stdlib.h -c "$f" -o "$(basename "${f%.c}").o"; done
for f in "$P/h264_run.c" "$P/h_alloc.c" streams.c "$HERE/bench/h264/main.c" "$G/ti_sdk_dl_config.c"; do "$CC/bin/tiarmclang" $CF -std=gnu99 -c "$f" -o "$(basename "${f%.c}").o"; done
"$CC/bin/tiarmclang" $ARCH ./*.o $LIBS ./linker.cmd -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,h264.map "-L$CC/lib" -llibc.a -o h264.out
echo "built $O/h264.out"; ls -l h264.out
