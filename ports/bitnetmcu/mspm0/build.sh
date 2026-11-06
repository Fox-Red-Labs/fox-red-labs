#!/bin/bash
# Build BitNetMCU for an MSPM0 LaunchPad with TI Arm Clang 5.1.1. Touches no hardware.
#   ./build.sh <board> <model> <opt> <clk> [ram]
#   ./build.sh --check [board]      only checks that every tool is found, builds nothing
#   board: g3507 | l1306    model: cnn_64 | cnn_16small | 12k | 1k    opt: O3 | Os    clk: 32 | 80 (80: G3507 only)
#   ram:   put the three inner loops in SRAM, as the upstream CH32V002 timings do
# Settings come from the environment (names shared with the other scripts, see ../recipe-env.sh). Set them first,
# for example:  export SDK="$HOME/ti/mspm0_sdk_2_11_00_07"
#   SDK    MSPM0 SDK folder     (default /Applications/ti/mspm0_sdk_2_11_00_07)
#   CC     TI Arm Clang folder  (default the copy inside CCStudio 21.0.1)
#   STACK  stack size in bytes  (default 3072; the L1306 needs 3600 for cnn_64: it has 4 kB of RAM in all)
#   OUT    build folder         (default ./build-<board>-<model>-<opt>-<clk>[-ram])
# Paths may contain spaces. Clock and pin setup come from the CoreMark builds of each board (TI SysConfig output, not edited).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../../.."
. "$HERE/../recipe-env.sh"
CHECK=0; if [ "$1" = "--check" ]; then CHECK=1; shift; B=$1; M=cnn_64; OPT=O3; CLK=${2:-}; else B=$1; M=$2; OPT=$3; CLK=$4; RAMF=$5; fi
case $B in
  g3507) GEN="$R/mspm0g3507/mcu-bench/baremetal/coremark/syscfg${CLK:-80}"; START=startup_mspm0g350x_ticlang.c ;;
  l1306) GEN="$R/mspm0l1306/bench/coremark/syscfg${CLK:-32}"; START=startup_mspm0l130x_ticlang.c ;;
  *) echo "board: g3507 or l1306"; exit 2 ;;
esac
[ $CHECK = 1 ] || case $M in cnn_64|cnn_16small|12k|1k) ;; *) echo "model: cnn_64, cnn_16small, 12k or 1k"; exit 2 ;; esac
br_need_path "SDK" "$SDK/source" "Install the MSPM0 SDK (https://www.ti.com/tool/download/MSPM0-SDK) and point SDK at its folder, e.g. export SDK=\"\$HOME/ti/mspm0_sdk_2_11_00_07\"."
br_need_exec "CC" "$CC/bin/tiarmclang" "Install TI Arm Clang (it comes with CCStudio 21.0.1) and point CC at the folder that holds bin/tiarmclang, e.g. export CC=\"/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS\"."
br_need_path "SysConfig output for $B at ${CLK:-the default clock} MHz" "$GEN/ti_msp_dl_config.c" "Board $B supports clock 32 (and 80 on g3507). The folder is tracked in this repository, so a missing file means an incomplete copy or an unsupported clock."
br_need_path "third-party model" "$R/third-party/bitnetmcu" "The model and upstream code ship inside this repository; re-clone it."
if [ $CHECK = 1 ]; then br_report; exit 0; fi
br_report quiet
if [ -n "${OUT:-}" ]; then mkdir -p "$OUT"; else OUT="$HERE/build-$B-$M-$OPT-$CLK${RAMF:+-ram}"; rm -rf "$OUT"; mkdir -p "$OUT"; fi
cd "$OUT"
# stack: the CNN keeps 2 kB of activations on it; the L1306 has 4 kB of RAM in all
sed -E "s/--stack_size=[0-9]+/--stack_size=${STACK:-3072}/" "$GEN/device_linker.cmd" > device_linker.cmd
ARCH=(-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb)
CFLAGS=("-$OPT" "${ARCH[@]}" "@$GEN/device.opt" "-I$GEN" "-I$HERE/.." "-I$R/third-party/bitnetmcu" "-I$R/mspm0l1306/bench/common"
 "-I$SDK/source/third_party/CMSIS/Core/Include" "-I$SDK/source" -gdwarf-3 -Wall -Wno-unused-variable
 "-DMODEL_H=\"BitNetMCU_model_$M.h\"" ${RAMF:+-DRAMFUNC})
"$CC/bin/tiarmclang" "${CFLAGS[@]}" -c "$HERE/main.c" -o main.o
"$CC/bin/tiarmclang" "${CFLAGS[@]}" -c "$GEN/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC/bin/tiarmclang" "${CFLAGS[@]}" -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o
"$CC/bin/tiarmclang" "${ARCH[@]}" -Wl,-u,_c_int00 *.o "-l$GEN/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,bitnet.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o bitnet.out
grep -E '^ +(FLASH|SRAM) ' bitnet.map
