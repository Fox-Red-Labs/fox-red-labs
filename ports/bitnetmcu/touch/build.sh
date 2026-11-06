#!/bin/bash
# Build the touch digit demo for the LP-MSPM0G3507 with the Waveshare 3.5 inch touch LCD wired as in
# mspm0g3507/display-test. Touches no hardware.
#   ./build.sh            -> out/digit_touch.out (and out/digit_touch.map)
#   ./build.sh --check    only checks that every tool is found, builds nothing
# Tool locations come from the environment (names shared with the other scripts, see ../recipe-env.sh).
# Set them in your terminal first, for example:  export SDK="$HOME/ti/mspm0_sdk_2_11_00_07"
#   SDK   MSPM0 SDK folder             (default /Applications/ti/mspm0_sdk_2_11_00_07)
#   CC    TI Arm Clang folder          (default the copy inside CCStudio 21.0.1)
#   GEN   SysConfig output to build on (default ../../../mspm0g3507/display-test/gen80, tracked in git:
#         80 MHz CPU, SPI1 at 20 MHz, the pins in the wiring table; regenerate it from display80.syscfg
#         with SysConfig only if you change a pin)
#   OUT   where the build goes         (default ./out)
# Paths may contain spaces. Checked on macOS 15.7 (Intel) only. The same compiler ships for Linux and Windows but this script was not run there.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../../.."
. "$HERE/../recipe-env.sh"
GEN="${GEN:-$R/mspm0g3507/display-test/${GEN_DIR:-gen80}}"
OUT="${OUT:-$HERE/out}"
br_need_path "SDK" "$SDK/source" "Install the MSPM0 SDK (https://www.ti.com/tool/download/MSPM0-SDK) and point SDK at its folder, e.g. export SDK=\"\$HOME/ti/mspm0_sdk_2_11_00_07\"."
br_need_exec "CC" "$CC/bin/tiarmclang" "Install TI Arm Clang (it comes with CCStudio 21.0.1, or as the standalone TI Arm Clang download) and point CC at the folder that holds bin/tiarmclang, e.g. export CC=\"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS\"."
br_need_path "GEN" "$GEN/ti_msp_dl_config.c" "GEN is the SysConfig output folder; the default is tracked in this repository, so a missing file means an incomplete copy of the repository."
br_need_path "third-party model" "$R/third-party/bitnetmcu" "The model and upstream code ship inside this repository; re-clone it."
if [ "$1" = "--check" ]; then br_report; exit 0; fi
br_report quiet
if [ "$OUT" = "$HERE/out" ]; then rm -rf "$OUT"; fi; mkdir -p "$OUT"; cd "$OUT"
sed -E "s/--stack_size=[0-9]+/--stack_size=4096/" "$GEN/device_linker.cmd" > device_linker.cmd
ARCH=(-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb)
CF=(-O3 "${ARCH[@]}" "@$GEN/device.opt" "-I$GEN" "-I$HERE" "-I$HERE/.." "-I$R/third-party/bitnetmcu" "-I$R/mspm0l1306/bench/common"
 "-I$SDK/source/third_party/CMSIS/Core/Include" "-I$SDK/source" -gdwarf-3 -Wall -Wno-unused-function -Wno-unused-variable
 -Wno-misleading-indentation "-DMODEL_H=\"BitNetMCU_model_cnn_64.h\"")
"$CC/bin/tiarmclang" "${CF[@]}" -c "$HERE/digit_touch.c" -o main.o
"$CC/bin/tiarmclang" "${CF[@]}" -c "$GEN/ti_msp_dl_config.c" -o cfg.o
"$CC/bin/tiarmclang" "${CF[@]}" -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c" -o st.o
"$CC/bin/tiarmclang" "${ARCH[@]}" -Wl,-u,_c_int00 main.o cfg.o st.o "-l$GEN/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,digit_touch.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o digit_touch.out
grep -E '^ +(FLASH|SRAM) ' digit_touch.map
echo "built $OUT/digit_touch.out"
