#!/bin/bash
# Build the Plaits bench for the LP-MSPM33C321A with TI Arm Clang (toolchain and SysConfig output as ports/lc3/target-m33/build.sh). Flashes nothing.
#   ./build.sh [O2|O3|Os] [hard|soft]   output $OUT/plaits-<opt>-<float>[-tag]/plaits.out (+ .map), size line printed
#   Float is always strict: -ffp-contract=off, no fast-math (the Mac reference is built the same way).
#   hard = FPU on (-mfpu=fpv5-sp-d16); soft = FPU not used (library float calls); the four driverlib sources the clock setup needs are rebuilt soft.
#   Env: OUT (default $PWD/out/synth/builds), G (SysConfig output, default out/llama2c/gen160),
#        STACK (default 8192), EXTRA (more compiler flags for the Plaits files), TAG (suffix of the output folder)
set -e
OPT=${1:-O2}; FL=${2:-hard}
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; TP="$REPO/third-party/plaits"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
OUT="${OUT:-$PWD/out/synth/builds}"; G="${G:-$PWD/out/llama2c/gen160}"
[ -f "$G/device_linker.cmd" ] || { echo "missing $G (run ports/llama2c/build.sh once to make the SysConfig output, or set G)"; exit 2; }
O="$OUT/plaits-$OPT-$FL${TAG:+-$TAG}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-8192}/" "$G/device_linker.cmd" > device_linker.cmd
HARD="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
if [ "$FL" = hard ]; then CODE="$HARD"; else CODE="-mcpu=cortex-m33+nofp -mthumb -mfloat-abi=soft"; fi
SYS="@$G/device.opt -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -ffunction-sections -fdata-sections"
CXX="-std=c++11 -fno-exceptions -fno-rtti -ffp-contract=off -include stdio.h -I$TP -I$PORT/common -w -ffunction-sections -fdata-sections -gdwarf-3"
CC1="$CC/bin/tiarmclang"
i=0; for f in $(find "$TP" -name '*.cc' | sort); do i=$((i+1)); "$CC1" -$OPT $CODE $CXX -DTEST $EXTRA -c "$f" -o "p${i}_$(basename ${f%.cc}).o" & if (( i % 8 == 0 )); then wait; fi; done; wait
"$CC1" -$OPT $CODE $CXX -DTEST $EXTRA -c "$PORT/common/plaitsrun.cc" -o plaitsrun.o
"$CC1" -$OPT $CODE $CXX $SYS $EXTRA -c "$PORT/target-m33/main.cc" -o main.o
"$CC1" -$OPT $CODE $SYS -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC1" -$OPT $CODE $SYS -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
if [ "$FL" = hard ]; then LIBS="-l$G/device.cmd.genlibs"; else
  for f in dl_common.c dl_timer.c dl_gpio.c m33/sysctl/dl_sysctl_mspm33c321x.c; do "$CC1" -$OPT $CODE $SYS -c "$SDK/source/ti/driverlib/$f" -o "drv_$(basename ${f%.c}).o"; done; LIBS=""; fi
"$CC1" $CODE -Wl,-u,_c_int00 ./*.o $LIBS "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,plaits.map -Wl,--unused_section_elimination=on -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc++.a -llibc++abi.a -llibc.a -o plaits.out
echo "built $O/plaits.out"; "$CC/bin/tiarmsize" plaits.out
