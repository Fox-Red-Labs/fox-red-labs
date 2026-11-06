#!/bin/bash
# Build the LC3 bench for the LP-MSPM33C321A with TI Arm Clang (toolchain and SysConfig output as ports/minimp3/target-m33/build.sh). Flashes nothing.
#   ./build.sh [O2|O3|Os] [hard|soft] [fast|strict]   output $OUT/lc3-<opt>-<float>-<fm>/lc3.out (+ .map), size line printed
#   hard = FPU on (-mfpu=fpv5-sp-d16); soft = FPU not used: the codec and the bench are compiled with -mfloat-abi=soft, so every
#     float operation is a library call; only the SysConfig output and the startup file (no float code) stay hard-float.
#   fast = upstream's -ffast-math; strict = -ffp-contract=off, no fast-math.
#   Env: OUT (default out/bench-day/lc3/builds), G (SysConfig output, default out/llama2c/gen160),
#        STACK (default 16384), EXTRA (more flags for the codec files)
set -e
OPT=${1:-O2}; FL=${2:-hard}; FM=${3:-fast}
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; L="$REPO/third-party/liblc3"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
OUT="${OUT:-$PWD/out/bench-day/lc3/builds}"; G="${G:-$PWD/out/llama2c/gen160}"
[ -f "$G/device_linker.cmd" ] || { echo "missing $G (run ports/llama2c/build.sh once to make the SysConfig output, or set G)"; exit 2; }
O="$OUT/lc3-$OPT-$FL-$FM${TAG:+-$TAG}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-16384}/" "$G/device_linker.cmd" > device_linker.cmd
HARD="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
if [ "$FL" = hard ]; then CODE="$HARD"; else CODE="-mcpu=cortex-m33+nofp -mthumb -mfloat-abi=soft"; fi
if [ "$FM" = fast ]; then FM_F="-ffast-math"; else FM_F="-ffp-contract=off"; fi
COMMON="@$G/device.opt -I$G -I$L/include -I$L/src -I$PORT/common -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -ffunction-sections -fdata-sections"
CC1="$CC/bin/tiarmclang"
SRCS="attdet bits bwdet energy lc3 ltpf mdct plc sns spec tables tns"
for s in $SRCS; do "$CC1" -$OPT $CODE $FM_F $COMMON $EXTRA -c "$L/src/$s.c" -o "$s.o"; done
"$CC1" -$OPT $CODE $FM_F $COMMON $EXTRA -c "$PORT/common/lc3run.c" -o lc3run.o
"$CC1" -$OPT $CODE $COMMON -c "$PORT/target-m33/main.c" -o main.o        # no float code in main: only integer reads of DWT
"$CC1" -$OPT $CODE $COMMON -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC1" -$OPT $CODE $COMMON -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
if [ "$FL" = hard ]; then LIBS="-l$G/device.cmd.genlibs"; else
  # TI's driverlib.a is built hard-float and the linker refuses to mix it with soft-float objects: build the four driverlib files the clock setup needs from source
  for f in dl_common.c dl_timer.c dl_gpio.c m33/sysctl/dl_sysctl_mspm33c321x.c; do "$CC1" -$OPT $CODE $COMMON -c "$SDK/source/ti/driverlib/$f" -o "drv_$(basename ${f%.c}).o"; done; LIBS=""; fi
"$CC1" $CODE -Wl,-u,_c_int00 ./*.o $LIBS "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,lc3.map -Wl,--unused_section_elimination=on -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o lc3.out
echo "built $O/lc3.out"; "$CC/bin/tiarmsize" lc3.out
