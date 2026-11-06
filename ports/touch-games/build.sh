#!/bin/bash
# Build the touch games with TI Arm Clang. Flashes nothing.
#   ./build.sh g3519          LP-MSPM0G3519 + Waveshare 3.5 in (the board it runs on)
#   ./build.sh g3507          LP-MSPM0G3507, same display (32 kB RAM: the tightest fit)
#   ./build.sh m33            LP-MSPM33C321A, core plus a screenless stub hal (build only: no touch display on that board)
#   ./build.sh all
# Options (environment): OPT=-Oz   SPI_SCR=0 (40 MHz, the default; 1 is 20 MHz)   BOOT_GAME=1 (start in 2048) 2 (minesweeper) 0 (menu)
#   BENCH=0 skips the measurement at boot   OUT=<dir> output folder (default out/touch-games)
# Output: $OUT/<board>/touch-games-<board>.out and .map; prints the size line.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-$PWD/out/touch-games}"
OPT="${OPT:--Oz}"; SPI_SCR="${SPI_SCR:-0}"; BOOT_GAME="${BOOT_GAME:-0}"; BENCH="${BENCH:-1}"
STACK="${STACK:-2048}"
COMMON="$HERE/src/app.c $HERE/src/gesture.c $HERE/src/gfx.c $HERE/src/font5x7.c $HERE/src/game2048.c $HERE/src/mines.c $HERE/src/ui.c"
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh

build_m0() {   # board device startup
  local B=$1 DEV=$2 STARTUP=$3
  . "$HERE/../../tools/frl/toolpaths.sh"
  local CLANG="$CC/bin/tiarmclang" G="$OUT/$B/gen" O="$OUT/$B" ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
  mkdir -p "$G" "$O"
  if [ ! -f "$G/ti_msp_dl_config.c" ] || [ "$HERE/syscfg/$B.syscfg" -nt "$G/ti_msp_dl_config.c" ]; then
    "$SYSCFG" -s "$SDK/.metadata/product.json" -d "$DEV" --compiler ticlang -o "$G" "$HERE/syscfg/$B.syscfg" >/dev/null
  fi
  local CF="$OPT $ARCH @$G/device.opt -I$G -I$HERE/src -I$HERE/boards -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -DBOARD_HEADER=\"board_$B.h\" -DBUILD_NAME=\"touch-games-$B\" -DSPI_SCR=$SPI_SCR -DBOOT_GAME=$BOOT_GAME -DBENCH=$BENCH -DGFX_BUF_PX=1536 \
 -ffunction-sections -fdata-sections -gdwarf-3 -Wall -Wno-unused-function"
  rm -f "$O"/*.o
  for f in $COMMON "$HERE/src/hal_mspm0.c" "$G/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP"; do
    "$CLANG" $CF -c "$f" -o "$O/$(basename "$f" .c).o"
  done
  sed -e "s/--stack_size=[0-9]*/--stack_size=$STACK/" "$G/device_linker.cmd" > "$O/linker.cmd"
  "$CLANG" $ARCH -Wl,-u,_c_int00 "$O"/*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/linker.cmd" \
     -Wl,-m,"$O/touch-games-$B.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$O/touch-games-$B.out"
  echo "== $B ($OPT, stack $STACK)"; "$CC/bin/tiarmsize" "$O/touch-games-$B.out" | tail -2
}

build_m33() {
  TOOLPATHS_FAMILY=mspm33-cc . "$HERE/../../tools/frl/toolpaths.sh"
  local SYSCFG33=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh O="$OUT/m33"
  rm -rf "$O"; mkdir -p "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output/." "$O/"
  cp "$HERE"/src/*.c "$HERE"/src/*.h "$O/"; rm -f "$O/hal_mspm0.c"
  cp "$HERE/boards/hal_stub_m33.c" "$O/gpio_toggle_output.c"      # the example's one source file becomes the stub hal and main
  python3 - "$O/ticlang/makefile" "$OPT" "$BOOT_GAME" "$BENCH" <<'PY'
import sys
p, opt, boot, bench = sys.argv[1:5]; s = open(p).read()
objs = " ".join(f"{n}.obj" for n in "app gesture gfx font5x7 game2048 mines ui".split())
s = s.replace("OBJECTS = gpio_toggle_output.obj", "OBJECTS = gpio_toggle_output.obj " + objs, 1)
s = s.replace("    -O2 \\", f"    {opt} -DBUILD_NAME=\\\"touch-games-m33\\\" -DBOOT_GAME={boot} -DBENCH={bench} -DGFX_BUF_PX=1536 -ffunction-sections -fdata-sections \\", 1)
s = s.replace("$(NAME).out: $(OBJECTS)", "%.obj: ../%.c\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n\n$(NAME).out: $(OBJECTS)", 1)
s = s.replace('"-Wl,-m,$(NAME).map" \\', '"-Wl,-m,$(NAME).map" -Wl,--unused_section_elimination=on \\', 1)
open(p, "w").write(s)
PY
  make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG33" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 || { tail -25 "$O/build.log"; exit 1; }
  # the example's 256-byte stack is below the 592 bytes measured on the G3519: raise it and relink
  sed -i '' "s/--stack_size=[0-9]*/--stack_size=1024/" "$O/ticlang/device_linker.cmd"; rm -f "$O/ticlang/gpio_toggle_output.out"
  ( cd "$O/ticlang" && "$CC/bin/tiarmclang" -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -Wl,-u,_c_int00 *.obj -ldevice.cmd.genlibs \
      "-L$SDK/source" -L.. device_linker.cmd -Wl,-m,gpio_toggle_output.map -Wl,--unused_section_elimination=on -Wl,--rom_model \
      "-L$CC/lib" -llibc.a -o gpio_toggle_output.out ) >> "$O/build.log" 2>&1 || { tail -25 "$O/build.log"; exit 1; }
  grep -q "stack_size=1024" "$O/ticlang/device_linker.cmd" || { echo "stack size change was overwritten"; exit 1; }
  echo "== m33 ($OPT, stub hal, stack 1024)"; "$CC/bin/tiarmsize" "$O/ticlang/gpio_toggle_output.out" | tail -2
}

case "${1:-all}" in
  g3519) build_m0 g3519 MSPM0G351X startup_mspm0g351x_ticlang.c ;;
  g3507) build_m0 g3507 MSPM0G350X startup_mspm0g350x_ticlang.c ;;
  m33)   build_m33 ;;
  all)   build_m0 g3519 MSPM0G351X startup_mspm0g351x_ticlang.c; build_m0 g3507 MSPM0G350X startup_mspm0g350x_ticlang.c; build_m33 ;;
  *) echo "usage: $0 g3519|g3507|m33|all"; exit 2 ;;
esac
