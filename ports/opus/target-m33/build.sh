#!/bin/bash
# Build the Opus bench for the LP-MSPM33C321A with TI Arm Clang (toolchain and SysConfig output as ports/lc3/target-m33/build.sh). Flashes nothing.
#   ./build.sh [fixed|float] [O2|O3|Os]     output $OUT/opus-<kind>-<opt>/opus.out (+ .map), size line printed
#   Env: FASTINT64=0|1 builds from a copy of the library with OPUS_FAST_INT64 forced to that value (libopus picks 1 for 64-bit hosts and 0 for this 32-bit core), OUT, G (SysConfig output, default out/llama2c/gen160), STACK (default 65536), EXTRA (more flags for the codec files), TAG
set -e
KIND=${1:-fixed}; OPT=${2:-O2}
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; O_="$REPO/third-party/opus"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"; . "$PORT/sources.sh"
OUT="${OUT:-$PWD/out/opus/builds}"; G="${G:-$PWD/out/llama2c/gen160}"
[ -f "$G/device_linker.cmd" ] || { echo "missing $G (run ports/llama2c/build.sh once to make the SysConfig output, or set G)"; exit 2; }
if [ -n "${FASTINT64:-}" ]; then
  C="$OUT/opus-int64-$FASTINT64-src"; rm -rf "$C"; mkdir -p "$C"; cp -R "$O_/include" "$O_/celt" "$O_/silk" "$O_/src" "$C/"
  sed -i.bak "s/^#define OPUS_FAST_INT64 [01]/#define OPUS_FAST_INT64 $FASTINT64/" "$C/celt/arch.h"
  srcs() { :; }; . "$PORT/sources.sh"; _s=$(srcs $KIND | sed "s#$O_#$C#"); O_="$C"
else _s=$(srcs $KIND); fi
B="$OUT/opus-$KIND-$OPT${FASTINT64:+-i64_$FASTINT64}${TAG:+-$TAG}"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-65536}/" "$G/device_linker.cmd" > device_linker.cmd
CODE="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
if [ "$KIND" = fixed ]; then K="-DOPUS_PORT_FIXED -I$O_/silk/fixed"; else K="-DOPUS_PORT_FLOAT -I$O_/silk/float"; fi
COMMON="@$G/device.opt -I$G -I$PORT/common -I$O_/include -I$O_/celt -I$O_/silk -I$O_/src -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -ffunction-sections -fdata-sections -ffp-contract=off -DHAVE_CONFIG_H -include $PORT/common/config.h $K"
CC1="$CC/bin/tiarmclang"; i=0
for s in $_s; do i=$((i+1)); "$CC1" -$OPT $CODE $COMMON -w $EXTRA -c "$s" -o "$(printf %03d $i)_$(basename ${s%.c}).o" & if (( i % 8 == 0 )); then wait; fi; done; wait
"$CC1" -$OPT $CODE $COMMON -w $EXTRA -c "$PORT/common/opusrun.c" -o opusrun.o
"$CC1" -$OPT $CODE $COMMON -c "$PORT/target-m33/main.c" -o main.o
"$CC1" -$OPT $CODE $COMMON -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC1" -$OPT $CODE $COMMON -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
"$CC1" $CODE -Wl,-u,_c_int00 ./*.o "-l$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,opus.map -Wl,--unused_section_elimination=on -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o opus.out
echo "built $B/opus.out"; "$CC/bin/tiarmsize" opus.out
