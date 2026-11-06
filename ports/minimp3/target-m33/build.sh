#!/bin/bash
# Build the minimp3 bench for the LP-MSPM33C321A with TI Arm Clang (toolchain and SysConfig output as ports/llama2c/build.sh). Flashes nothing.
#   ./build.sh [O2|O3|Os] [hard|soft]     output $OUT/mp3-<opt>-<float>/mp3.out (+ .map), size line printed
#   Env: OUT (default out/minimp3/builds), G (SysConfig output, default out/llama2c/gen160), STACK (default 32768), EXTRA (more compiler flags)
set -e
OPT=${1:-O2}; FL=${2:-hard}
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
OUT="${OUT:-$PWD/out/minimp3/builds}"; G="${G:-$PWD/out/llama2c/gen160}"
[ -f "$G/device_linker.cmd" ] || { echo "missing $G (run ports/llama2c/build.sh once to make the SysConfig output, or set G)"; exit 2; }
O="$OUT/mp3-$OPT-$FL"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
python3 -I "$PORT/tools/embed_mp3.py" "$PORT/clips/stereo128.mp3" "$PORT/clips/mono64.mp3" clips.c
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-32768}/" "$G/device_linker.cmd" > device_linker.cmd
if [ "$FL" = hard ]; then FP="-mfloat-abi=hard -mfpu=fpv5-sp-d16"; else FP="-mfloat-abi=soft"; fi
ARCH="-mcpu=cortex-m33 -mthumb $FP"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$PORT/common -I$REPO/third-party/minimp3 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -ffunction-sections -fdata-sections $EXTRA"
CC1="$CC/bin/tiarmclang"
for f in "$PORT/common/mp3run.c" "$PORT/common/mp3bench.c" "$PORT/target-m33/main.c" clips.c "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  "$CC1" $CF -c "$f" -o "$(basename "${f%.c}").o"
done
"$CC1" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,mp3.map -Wl,--unused_section_elimination=on -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o mp3.out
echo "built $O/mp3.out"; "$CC/bin/tiarmsize" mp3.out
