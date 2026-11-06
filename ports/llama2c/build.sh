#!/bin/bash
# Build the TinyStories 260K int8 demo for the LP-MSPM33C321A with TI Arm Clang (toolchain and SysConfig output as ports/doom/target/build.sh).
# Flashes nothing.
#   ./build.sh [opt: O2|O3|Os] [emu]        output $OUT/llama2c-<opt>[-emu]/llama2c.out (+ .map), size line printed
#   Env: GS (quantization group size, default 4), STEPS (longest sequence, default 128), PROMPT, TEMP, TOPP, SEED,
#        OUT (default out/llama2c/builds), G (SysConfig output folder, default $OUT/../gen160, made if missing),
#        PYTHON (a python3 with numpy), MODEL (folder with stories260K.bin and tok512.bin, default ports/llama2c/model-cache, fetched if missing)
# "emu" builds the variant for the Cortex-M33 emulator: no clock setup, no cycle counter.
set -e
OPT=${1:-O2}; EMU=${2:-}
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
OUT="${OUT:-$PWD/out/llama2c/builds}"; G="${G:-$OUT/../gen160}"; MODEL="${MODEL:-$HERE/model-cache}"; GS="${GS:-4}"; STEPS="${STEPS:-128}"
"$HERE/fetch.sh" "$MODEL"   # checks both files on every build; downloads only what is missing or wrong
if [ ! -f "$G/device_linker.cmd" ]; then   # the SysConfig output of TI's sysctl_mclk_syspll example: 160 MHz SYSPLL, as mspm33c321a/prebuild.sh makes it
  EXD="$SDK/examples/nortos/LP_MSPM33C321A/driverlib/sysctl_mclk_syspll"; mkdir -p "$G"
  /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$EXD/sysctl_mclk_syspll.syscfg" >/dev/null
  cp "$EXD/ticlang/device_linker.cmd" "$G/device_linker.cmd"
fi
O="$OUT/llama2c-$OPT${EMU:+-emu}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
"${PYTHON:-python3}" -I "$HERE/tools/quantize_q80.py" "$MODEL/stories260K.bin" q80.bin "$GS"   # needs numpy
python3 -I "$HERE/tools/embed.py" q80.bin "$MODEL/tok512.bin" blobs.c
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-4096}/" "$G/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
D="-DLL_STEPS=$STEPS $EXTRA"; [ -n "$PROMPT" ] && D="$D -DLL_PROMPT=\"$PROMPT\""; [ -n "$TEMP" ] && D="$D -DLL_TEMP=$TEMP"; [ -n "$TOPP" ] && D="$D -DLL_TOPP=$TOPP"; [ -n "$SEED" ] && D="$D -DLL_SEED=$SEED"
[ -n "$EMU" ] && D="$D -DLL_EMU"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$HERE/src -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -ffunction-sections -fdata-sections $D"
CC1="$CC/bin/tiarmclang"
if [ -n "$EMU" ]; then "$CC1" $CF -c "$HERE/src/llama2q.c" -o llama2q.o
else "$CC1" $CF -include "$HERE/target-m33/cycles.h" -c "$HERE/src/llama2q.c" -o llama2q.o; fi
"$CC1" $CF -c "$HERE/target-m33/main.c" -o main.o
"$CC1" $CF -c blobs.c -o blobs.o
[ -z "$EMU" ] && "$CC1" $CF -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC1" $CF -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
"$CC1" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,llama2c.map -Wl,--unused_section_elimination=on -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o llama2c.out
echo "built $O/llama2c.out"; "$CC/bin/tiarmsize" llama2c.out
