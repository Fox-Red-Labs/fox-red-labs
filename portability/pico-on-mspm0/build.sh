#!/bin/bash
# Build one raspberrypi pico-examples program, unchanged, for the LP-MSPM0G3519.   build.sh <example | folder with the .c files> [outdir]
# Variables: SDK MSPM0 SDK 2.11.00.07, CGT TI Arm Clang 5.1.1.LTS folder, SYSCFG sysconfig_cli.sh (1.28.0 or 1.26.3),
#   PICO_SDK a pico-sdk clone (2.3.1; only two of its headers are used), OUT build folder (README: Set up once).
#   example: blink_simple | hello_serial | bus_scan | led_fade | hello_adc   (the folder name under third-party/pico-examples/)
# Our include/ comes first on the include path, then the real Pico SDK's header folders; the example's .c file is compiled as
# published. Runs SysConfig on boards/mspm0g3519/board.syscfg, compiles app + layer + generated config + TI's startup file, links,
# prints the size. Stack 2 kB, the Pico SDK default (PICO_STACK_SIZE 0x800).
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
ARG="${1:?example name or folder}"; EX="$(basename "$ARG")"
HERE="$(cd "$(dirname "$0")" && pwd)"
O="${2:-${OUT:-$PWD/out/pico/build-$EX}}"
SDK="${SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}"
CGT="${CGT:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}"
SYSCFG="${SYSCFG:-/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh}"; [ -x "$SYSCFG" ] || SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
PICO_SDK="${PICO_SDK:-$HOME/pico-sdk}"
need SDK "$SDK/source/ti/devices/msp"; need CGT "$CGT/bin/tiarmclang"; need SYSCFG "$SYSCFG"; need OUT "$(dirname "$O")"
need PICO_SDK "$PICO_SDK/src/common/pico_base_headers/include/pico/error.h"
if [ -d "$ARG" ]; then APP="$(cd "$ARG" && pwd)"; else APP="$HERE/../../third-party/pico-examples/$EX"; fi; need example "$APP"
BOARD="$HERE/boards/mspm0g3519"
LAYER=("$HERE"/src/*.c "$BOARD/pins.c")
rm -rf "$O"; mkdir -p "$O/gen"
{
  "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$O/gen" "$BOARD/board.syscfg"
  CF=(-I"$HERE/include" -I"$BOARD" -I"$PICO_SDK/src/common/pico_base_headers/include" -I"$PICO_SDK/src/common/pico_binary_info/include"
      -I"$HERE/src" -I"$O/gen" @"$O/gen/device.opt" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source"
      ${PICO_SHIM_CFLAGS:-} -Os -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -gdwarf-3)
  for f in "$APP"/*.c "${LAYER[@]}" "$O/gen/ti_msp_dl_config.c" \
           "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c"; do
    "$CGT/bin/tiarmclang" "${CF[@]}" -c "$f" -o "$O/$(basename "${f%.c}").o"
  done
  "$CGT/bin/tiarmclang" -mcpu=cortex-m0plus -march=thumbv6m -mthumb -Wl,-u,_c_int00 "$O"/*.o \
    -Wl,-l"$O/gen/device.cmd.genlibs" -L"$SDK/source" "$O/gen/device_linker.cmd" -Wl,--stack_size=0x800 \
    -Wl,-m,"$O/$EX.map" -Wl,--rom_model -Wl,--warn_sections -L"$CGT/lib" -llibc.a -o "$O/$EX.out"
} 2>&1 | tee "$O/build.log"
"$CGT/bin/tiarmsize" "$O/$EX.out"
