#!/bin/bash
# Build one Renesas ra-fsp-examples project for the LP-MSPM0G3519.   build.sh <example> [outdir]
# Variables: SDK MSPM0 SDK 2.11.00.07, CGT TI Arm Clang 5.1.1.LTS folder, SYSCFG sysconfig_cli.sh (1.26.3 or 1.28.0), OUT build folder
#   (README: Set up once).
#   example: iic_master | sci_uart | adc | gpt | adc_gpt_periodic_sampling   (the folder name under third-party/renesas/)
# Copies the unchanged Renesas src, applies ports/renesas-ra-fsp/patches/<example>.patch if there is one, runs SysConfig on
# boards/mspm0g3519/board.syscfg, compiles app + shim + generated config + TI's startup file, links, prints the size.
set -euo pipefail
need() { [ -e "$2" ] || { echo "error: $1 not found at $2 (set $1, see README: Set up once)" >&2; exit 1; }; }
EX="${1:?example folder under third-party/renesas/}"
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
O="${2:-${OUT:-$PWD/out/renesas/build-$EX}}"
SDK="${SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}"
CGT="${CGT:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}"
SYSCFG="${SYSCFG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}"; [ -x "$SYSCFG" ] || SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
need SDK "$SDK/source/ti/devices/msp"; need CGT "$CGT/bin/tiarmclang"; need SYSCFG "$SYSCFG"; need OUT "$(dirname "$O")"
SRC="$REPO/third-party/renesas/$EX/src"; PATCH="$REPO/ports/renesas-ra-fsp/patches/$EX.patch"
BOARD="$HERE/boards/mspm0g3519"; CFG="$BOARD/board.syscfg"; [ -f "$BOARD/$EX.syscfg" ] && CFG="$BOARD/$EX.syscfg"
LAYER=("$HERE/src/fsp_mspm0.c" "$BOARD/hal_data.c"); EXF=(-DFSP_EX_"$EX")
case "$EX" in                                   # the timer file serves sci_uart's PWM too; the DMA and ELC file only the group scan
  sci_uart|gpt) LAYER+=("$HERE/src/fsp_mspm0_gpt.c");;
  adc_gpt_periodic_sampling) LAYER+=("$HERE/src/fsp_mspm0_gpt.c" "$HERE/src/fsp_mspm0_adcgpt.c"); EXF+=(-DFSP_ADC_GROUP);;
esac
rm -rf "$O"; mkdir -p "$O/gen" "$O/app"; cp "$SRC"/*.[ch] "$O/app/"
[ -f "$PATCH" ] && [ -z "${NO_PATCH:-}" ] && (cd "$O/app" && patch -p1 < "$PATCH")
{
  "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$O/gen" "$CFG"
  CF=(-I"$O/gen" -I"$HERE/include" -I"$O/app" @"$O/gen/device.opt" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source"
      ${EXF[@]} ${FSP_SHIM_CFLAGS:-} -Os -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -Wno-unused-variable -gdwarf-3)
  for f in "$O"/app/*.c "${LAYER[@]}" "$O/gen/ti_msp_dl_config.c" \
           "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c"; do
    "$CGT/bin/tiarmclang" "${CF[@]}" -c "$f" -o "$O/$(basename "${f%.c}").o"
  done
  "$CGT/bin/tiarmclang" -mcpu=cortex-m0plus -march=thumbv6m -mthumb -Wl,-u,_c_int00 "$O"/*.o \
    -Wl,-l"$O/gen/device.cmd.genlibs" -L"$SDK/source" "$O/gen/device_linker.cmd" \
    -Wl,-m,"$O/$EX.map" -Wl,--rom_model -Wl,--warn_sections -L"$CGT/lib" -llibc.a -o "$O/$EX.out"
} 2>&1 | tee "$O/build.log"
"$CGT/bin/tiarmsize" "$O/$EX.out"
