#!/bin/bash
# Build the ZX Spectrum 48K emulator for the LP-MSPM0G3519 + Waveshare 3.5 inch display. Flashes nothing.
#   ./build.sh                 ROM-less test image (no Sinclair code), or the reader's ROM:  ZX_ROM=/path/to/48.rom ./build.sh
# Options (environment): RAMFUNC=1 runs the Z80 core from the second SRAM bank  OPT=-O2 (the emulator core wants speed; -Oz is much smaller and slower)  STOP_AFTER=n (stop at a frame boundary)  OUT=<dir>
# Output: $OUT/g3519/zx-g3519.out, .bin (padded to 16 bytes for DSLite) and .map. The display layer is the one in ports/touch-games.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; TG="$HERE/../touch-games"; CH="$HERE/../../third-party/chips"
OUT="${OUT:-$PWD/out/zx-spectrum}"; OPT="${OPT:--O2}"; STOP_AFTER="${STOP_AFTER:-0}"; STACK=2048; RAMFUNC="${RAMFUNC:-0}"
. "$HERE/../../tools/frl/toolpaths.sh"
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CLANG="$CC/bin/tiarmclang"; G="$OUT/gen"; O="$OUT/g3519"; ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
mkdir -p "$G" "$O"
python3 "$HERE/tools/mkrom.py" "$G/rom.h" "$OUT/rom.bin" ${ZX_ROM:+"$ZX_ROM"}
if [ ! -f "$G/ti_msp_dl_config.c" ] || [ "$TG/syscfg/g3519.syscfg" -nt "$G/ti_msp_dl_config.c" ]; then
  "$SYSCFG" -s "$SDK/.metadata/product.json" -d MSPM0G351X --compiler ticlang -o "$G" "$TG/syscfg/g3519.syscfg" >/dev/null
fi
CF="$OPT $ARCH @$G/device.opt -I$G -I$HERE/src -I$CH -I$TG/src -I$TG/boards -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -DBOARD_HEADER=\"board_g3519.h\" -DBUILD_NAME=\"zx-g3519\" -DSPI_SCR=0 -DSTOP_AFTER_FRAMES=$STOP_AFTER $([ "$RAMFUNC" = 1 ] && echo -DZX_RAMFUNC) -DGFX_BUF_PX=2048 \
 -ffunction-sections -fdata-sections -gdwarf-3 -Wall -Wno-unused-function"
rm -f "$O"/*.o
for f in "$HERE/src/app.c" "$HERE/src/zx.c" "$TG/src/gfx.c" "$TG/src/font5x7.c" "$TG/src/hal_mspm0.c" "$G/ti_msp_dl_config.c" \
         "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c"; do
  "$CLANG" $CF -c "$f" -o "$O/$(basename "$f" .c).o"
done
sed -e "s/--stack_size=[0-9]*/--stack_size=$STACK/" -e "s/run=SRAM_BANK0, table(BINIT)/run=SRAM_BANK1, table(BINIT)/" "$G/device_linker.cmd" > "$O/linker.cmd"
"$CLANG" $ARCH -Wl,-u,_c_int00 "$O"/*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/linker.cmd" \
   -Wl,-m,"$O/zx-g3519.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$O/zx-g3519.out"
"$CC/bin/tiarmobjcopy" -O binary "$O/zx-g3519.out" "$O/zx-g3519.bin"
python3 - "$O/zx-g3519.bin" <<'PY'
import sys; p = sys.argv[1]; b = open(p, "rb").read(); b += b"\xff" * (-len(b) % 16); open(p, "wb").write(b)
PY
echo "== zx g3519 ($OPT, stack $STACK)"; "$CC/bin/tiarmsize" "$O/zx-g3519.out" | tail -2
