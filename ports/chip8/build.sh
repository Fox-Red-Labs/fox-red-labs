#!/bin/bash
# Build the CHIP-8 interpreter for the LP-MSPM0G3519 + Waveshare 3.5 inch display. Flashes nothing.
#   ./build.sh                 uses ROMs from $ROMS (default out/chip8/roms, filled by tools/fetch_tests.sh) plus anything in ports/chip8/roms/*.ch8
# Options (environment): OPT=-Oz  BOOT_ROM=n (index of the ROM shown at start)  BENCH=0 skips the speed run at boot  OUT=<dir>
# Output: $OUT/chip8-g3519.out, .bin (padded to 16 bytes, the way DSLite flashes it) and .map; prints the size line.
# The display layer is the one in ports/touch-games (shared once): same hal, same SysConfig file, same board header.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; TG="$HERE/../touch-games"
OUT="${OUT:-$PWD/out/chip8}"; ROMS="${ROMS:-$PWD/out/chip8/roms}"
OPT="${OPT:--Oz}"; BOOT_ROM="${BOOT_ROM:-0}"; BENCH="${BENCH:-1}"; STACK=1024
. "$HERE/../../tools/frl/toolpaths.sh"
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CLANG="$CC/bin/tiarmclang"; G="$OUT/gen"; O="$OUT/g3519"; ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
mkdir -p "$G" "$O"
python3 "$HERE/tools/mkroms.py" "$G/roms.h" "$ROMS"/[1-9]-*.ch8 "$HERE"/roms/*.ch8
if [ ! -f "$G/ti_msp_dl_config.c" ] || [ "$TG/syscfg/g3519.syscfg" -nt "$G/ti_msp_dl_config.c" ]; then
  "$SYSCFG" -s "$SDK/.metadata/product.json" -d MSPM0G351X --compiler ticlang -o "$G" "$TG/syscfg/g3519.syscfg" >/dev/null
fi
CF="$OPT $ARCH @$G/device.opt -I$G -I$HERE/src -I$TG/src -I$TG/boards -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -DBOARD_HEADER=\"board_g3519.h\" -DBUILD_NAME=\"chip8-g3519\" -DSPI_SCR=0 -DBOOT_ROM=$BOOT_ROM -DBENCH=$BENCH -DGFX_BUF_PX=1600 \
 -ffunction-sections -fdata-sections -gdwarf-3 -Wall -Wno-unused-function"
rm -f "$O"/*.o
for f in "$HERE/src/app.c" "$HERE/src/chip8.c" "$TG/src/gfx.c" "$TG/src/font5x7.c" "$TG/src/hal_mspm0.c" "$G/ti_msp_dl_config.c" \
         "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c"; do
  "$CLANG" $CF -c "$f" -o "$O/$(basename "$f" .c).o"
done
sed -e "s/--stack_size=[0-9]*/--stack_size=$STACK/" "$G/device_linker.cmd" > "$O/linker.cmd"
"$CLANG" $ARCH -Wl,-u,_c_int00 "$O"/*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/linker.cmd" \
   -Wl,-m,"$O/chip8-g3519.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$O/chip8-g3519.out"
"$CC/bin/tiarmobjcopy" -O binary "$O/chip8-g3519.out" "$O/chip8-g3519.bin"
python3 - "$O/chip8-g3519.bin" <<'PY'
import sys; p = sys.argv[1]; b = open(p, "rb").read(); b += b"\xff" * (-len(b) % 16); open(p, "wb").write(b)
PY
echo "== chip8 g3519 ($OPT, stack $STACK)"; "$CC/bin/tiarmsize" "$O/chip8-g3519.out" | tail -2
