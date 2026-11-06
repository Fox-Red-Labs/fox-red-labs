#!/bin/bash
# Build LVGL v9.6.0 + the demo for the LP-MSPM0G3507 (default) or the LP-MSPM0G3519 with TI Arm Clang. Does not flash anything.
#   BOARD=mspm0g3519 ./build.sh    the G3519: SysConfig, pin header and gen dirs from boards/mspm0g3519/, output in out/mspm0g3519/
#   ./build.sh                  the three flashable builds (they fit the 128 KB flash)
#   ./build.sh lvgl_32mhz       one of them
#   ./build.sh oversize         the full-feature demo (Montserrat 20 added), linked into a 512 KB
#                               pretend flash only to measure it. NOT flashable: does not fit the chip.
#   OPT=-Os ./build.sh          change optimization (default -Oz)
#   ROWS=20 MEM_KB=10 STACK=3072 ./build.sh    draw buffer rows, LVGL heap (KB), stack (bytes)
# Output: out/<config>/<config>.out and .map. LVGL is compiled once per variant into out/obj_<variant>.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
LVGL="$HERE/../../third-party/lvgl"; export LVGL   # the vendored LVGL copy, shared by every board
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
OPT="${OPT:--Oz}"; ROWS="${ROWS:-10}"; MEM_KB="${MEM_KB:-12}"; STACK="${STACK:-6144}"
BOARD="${BOARD:-mspm0g3507}"
if [ "$BOARD" = mspm0g3519 ]; then
  SC="$HERE/boards/$BOARD"; GENROOT="$SC"; DEVICE=MSPM0G351X; STARTUP=startup_mspm0g351x_ticlang.c; BOARDINC="-include $SC/board_pins.h"
  OUT="$HERE/out/$BOARD"; FLASH_FIT=0x00080000; FLASH_FULL=0x00080000
else
  SC="$HERE/syscfg"; GENROOT="$HERE"; DEVICE=MSPM0G350X; STARTUP=startup_mspm0g350x_ticlang.c; BOARDINC=""
  OUT="$HERE/out"; FLASH_FIT=0x00020000; FLASH_FULL=0x00080000
fi
mkdir -p "$OUT"
CLANG="$CC/bin/tiarmclang"

# name : SysConfig file : generated dir : SPI clock divider : variant : flash length
CONFIGS="lvgl_32mhz:lvgl:gen_lvgl:0:fit:$FLASH_FIT
lvgl_80mhz_spi40:lvgl80:gen_lvgl80:0:fit:$FLASH_FIT
lvgl_80mhz_spi20:lvgl80:gen_lvgl80:1:fit:$FLASH_FIT
oversize_32mhz_DO_NOT_FLASH:lvgl:gen_lvgl:0:full:$FLASH_FULL"
WANT="${1:-default}"
want() { case "$WANT" in default) case "$1" in oversize*) return 1;; *) return 0;; esac;;
                         oversize) case "$1" in oversize*) return 0;; *) return 1;; esac;;
                         *) [ "$WANT" = "$1" ];; esac; }

# 1. SysConfig output, regenerated when the .syscfg is newer
for c in $CONFIGS; do IFS=: read -r name cfg gen scr variant flen <<<"$c"
  if [ ! -f "$GENROOT/$gen/ti_msp_dl_config.c" ] || [ "$SC/$cfg.syscfg" -nt "$GENROOT/$gen/ti_msp_dl_config.c" ]; then
    mkdir -p "$GENROOT/$gen"
    "$SYSCFG" -s "$SDK/.metadata/product.json" -d $DEVICE --compiler ticlang -o "$GENROOT/$gen" "$SC/$cfg.syscfg" >/dev/null
  fi
done

# 2. LVGL sources, once per variant. No drivers, no third-party libs: the config switches them off anyway.
( cd "$LVGL/src"; find . -name '*.c' -not -path './drivers/*' -not -path './libs/*' | sort ) > "$OUT/lvgl_files.txt"
build_lvgl() { variant="$1"; OBJ="$OUT/obj_$variant"; mkdir -p "$OBJ"
  DEF=""; [ "$variant" = full ] && DEF="-DDEMO_FULL"
  LVF="$OPT $ARCH $DEF -DLV_CONF_INCLUDE_SIMPLE -DLV_MEM_SIZE=$((MEM_KB*1024)) -I$HERE -I$LVGL -ffunction-sections -fdata-sections -Wall -Wno-unused-parameter -Wno-unused-function -gdwarf-3"
  echo "LVGL ($variant): $OPT, heap ${MEM_KB} KB"
  echo "$LVF $(shasum "$HERE/lv_conf.h" | cut -c1-12) $(cat "$HERE"/size-patch.diff | shasum | cut -c1-12)" > "$OBJ/flags.new"
  if ! cmp -s "$OBJ/flags.new" "$OBJ/flags.txt" 2>/dev/null; then rm -f "$OBJ"/lvgl_*.o; fi
  mv "$OBJ/flags.new" "$OBJ/flags.txt"
  export -f compile_one; export CLANG LVF HERE OBJ
  xargs -P 6 -I{} bash -c 'compile_one {}' < "$OUT/lvgl_files.txt"
}
compile_one() { f="$1"; o="$OBJ/lvgl_$(echo "${f#./}" | tr '/' '_' | sed 's/\.c$/.o/')"
  [ -f "$o" ] || "$CLANG" $LVF -c "$LVGL/src/${f#./}" -o "$o"; }
BUILT=""

# 3. Port and app, then link each config
for c in $CONFIGS; do IFS=: read -r name cfg gen scr variant flen <<<"$c"
  want "$name" || continue
  case " $BUILT " in *" $variant "*) ;; *) build_lvgl "$variant"; BUILT="$BUILT $variant";; esac
  OBJ="$OUT/obj_$variant"; DEF=""; [ "$variant" = full ] && DEF="-DDEMO_FULL"
  G="$GENROOT/$gen"; O="$OUT/$name"; mkdir -p "$O"; rm -f "$O"/*.o
  CF="$OPT $ARCH $DEF $BOARDINC @$G/device.opt -I$G -I$HERE -I$LVGL -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -DLV_CONF_INCLUDE_SIMPLE -DLV_MEM_SIZE=$((MEM_KB*1024)) -DSPI_SCR=$scr -DDRAW_ROWS=$ROWS -ffunction-sections -fdata-sections -gdwarf-3 -Wall"
  for s in main port app; do "$CLANG" $CF -c "$HERE/src/$s.c" -o "$O/$s.o"; done
  "$CLANG" $CF -c "$G/ti_msp_dl_config.c" -o "$O/cfg.o"
  "$CLANG" $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP" -o "$O/st.o"
  sed -e "s/--stack_size=[0-9]*/--stack_size=$STACK/" -e "s/length = 0x00020000/length = ${FLASH_LEN:-$flen}/" "$G/device_linker.cmd" > "$O/linker.cmd"
  "$CLANG" $ARCH -Wl,-u,_c_int00 "$O"/main.o "$O"/port.o "$O"/app.o "$O"/cfg.o "$O"/st.o "$OBJ"/lvgl_*.o \
     -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/linker.cmd" \
     -Wl,-m,"$O/$name.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$O/$name.out"
  echo "== $name"; "$CC/bin/tiarmsize" "$O/$name.out" | tail -1
done
