#!/bin/bash
# Build the thermostat screen (LVGL 9.6 + radar parser + status line) for the LP-MSPM0G3519 with TI Arm Clang. Flashes nothing.
#   ./build.sh            -> out/therm/therm.out (and .map), prints the size line
#   OPT=-Os ./build.sh    change optimization (default -Oz)
# Reuses the LVGL port layer (src/main.c, src/port.c) from mspm0g3507/lvgl-g3507, the vendored LVGL in third-party/lvgl
# and the radar parser from am13e230/radar-parser. Our own files: src/therm.c, therm.syscfg, lv_conf.h (Barlow fonts and the logo image on, Montserrat 20 and 48 off).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
L="$ROOT/mspm0g3507/lvgl-g3507"; LVGL="$ROOT/third-party/lvgl"; PARSER="$ROOT/am13e230/radar-parser"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
OPT="${OPT:--Oz}"; ROWS="${ROWS:-10}"; MEM_KB="${MEM_KB:-28}"; STACK="${STACK:-6144}"
OUT="${OUT:-$HERE/out}"; O="$OUT/therm"; OBJ="$OUT/obj_lvgl"; G="$HERE/gen"
CLANG="$CC/bin/tiarmclang"
avail=$(df -k /System/Volumes/Data | awk 'NR==2{print int($4/1024)}'); [ "$avail" -ge "${DISK_GATE_MB:-2048}" ] || { echo "disk gate: only ${avail} MB free, need ${DISK_GATE_MB:-2048}"; exit 2; }
mkdir -p "$O" "$OBJ" "$G"
[ -f "$G/ti_msp_dl_config.c" ] && [ ! "$HERE/therm.syscfg" -nt "$G/ti_msp_dl_config.c" ] || \
  "$SYSCFG" -s "$SDK/.metadata/product.json" -d MSPM0G351X --compiler ticlang -o "$G" "$HERE/therm.syscfg" >/dev/null
( cd "$LVGL/src"; find . -name '*.c' -not -path './drivers/*' -not -path './libs/*' | sort ) > "$OUT/lvgl_files.txt"
LVF="$OPT $ARCH -DLV_CONF_INCLUDE_SIMPLE -DLV_MEM_SIZE=$((MEM_KB*1024)) -I$HERE -I$LVGL -ffunction-sections -fdata-sections -Wall -Wno-unused-parameter -Wno-unused-function -gdwarf-3"
echo "$LVF $(shasum "$HERE/lv_conf.h" | cut -c1-12) $(shasum "$L/size-patch.diff" | cut -c1-12)" > "$OBJ/flags.new"
cmp -s "$OBJ/flags.new" "$OBJ/flags.txt" 2>/dev/null || rm -f "$OBJ"/lvgl_*.o
mv "$OBJ/flags.new" "$OBJ/flags.txt"
compile_one() { f="$1"; o="$OBJ/lvgl_$(echo "${f#./}" | tr '/' '_' | sed 's/\.c$/.o/')"; [ -f "$o" ] || "$CLANG" $LVF -c "$LVGL/src/${f#./}" -o "$o"; }
export -f compile_one; export CLANG LVF OBJ LVGL
xargs -P 6 -I{} bash -c 'compile_one {}' < "$OUT/lvgl_files.txt"
for f in "$HERE"/fonts/*.c; do n=$(basename "$f" .c); [ "$f" -ot "$OBJ/lvgl_font_$n.o" ] 2>/dev/null || "$CLANG" $LVF -c "$f" -o "$OBJ/lvgl_font_$n.o"; done
BOARDINC="-include $L/boards/mspm0g3519/board_pins.h"
CF="$OPT $ARCH $BOARDINC @$G/device.opt -I$G -I$HERE -I$HERE/src -I$L/src -I$LVGL -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -DLV_CONF_INCLUDE_SIMPLE -DLV_MEM_SIZE=$((MEM_KB*1024)) -DSPI_SCR=0 -DDRAW_ROWS=$ROWS -ffunction-sections -fdata-sections -gdwarf-3 -Wall"
rm -f "$O"/*.o
"$CLANG" $CF -c "$L/src/main.c" -o "$O/main.o"; "$CLANG" $CF -c "$L/src/port.c" -o "$O/port.o"
"$CLANG" $CF -c "$HERE/src/therm.c" -o "$O/therm.o"; "$CLANG" $CF -c "$HERE/src/tlv_parser.c" -o "$O/tlv.o"; "$CLANG" $CF -c "$HERE/src/frl_logo.c" -o "$O/logo.o"
"$CLANG" $CF -c "$G/ti_msp_dl_config.c" -o "$O/cfg.o"
"$CLANG" $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c" -o "$O/st.o"
sed -e "s/--stack_size=[0-9]*/--stack_size=$STACK/" "$G/device_linker.cmd" > "$O/linker.cmd"
"$CLANG" $ARCH -Wl,-u,_c_int00 "$O"/main.o "$O"/port.o "$O"/therm.o "$O"/tlv.o "$O"/logo.o "$O"/cfg.o "$O"/st.o "$OBJ"/lvgl_*.o \
   -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/linker.cmd" -Wl,-m,"$O/therm.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$O/therm.out"
"$CC/bin/tiarmsize" "$O/therm.out" | tail -2
