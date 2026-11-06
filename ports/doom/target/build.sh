#!/bin/bash
# Doom engine (doomgeneric, GPL-2.0) for the LP-MSPM33C321A with TI Arm Clang, the same toolchain and SysConfig output as mspm33c321a/tools/build_h264.sh.
#   build.sh <opt: Os|O2|O3> <zone KiB> [extra compile flags]       output $OUT/doom-<opt>-z<zone>/doom.out and doom.map
#   Env: SRC (patched engine source, default ports/doom/src-upstream/doomgeneric), G (SysConfig gen160 folder), EMU=1 builds the emulator variant
#   (no SysConfig init, console and timer through two registers the host harness provides), STACK (bytes, default 8192), HEAP (bytes, default 2048).
# Nothing here flashes a board.
set -e
OPT=${1:-Os}; ZKB=${2:-160}; shift 2 || true; X="$*"
HERE="$(cd "$(dirname "$0")" && pwd)"; DOOM="$(cd "$HERE/.." && pwd)"; REPO="$(cd "$DOOM/../.." && pwd)"
SRC="${SRC:-$DOOM/src-upstream/doomgeneric}"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"
R="${R:-$PWD/out/mspm33c321a}"; G="${G:-$R/gen160}"
[ -f "$G/device_linker.cmd" ] || { echo "No SysConfig output at $G: run mspm33c321a/prebuild.sh first, or set G to a folder that holds device_linker.cmd, device.opt, device.cmd.genlibs and ti_msp_dl_config.c"; exit 2; }
OUTBASE="${OUT:-$PWD/out/doom/target-builds}"; TAG="${TAG:-}"
O="$OUTBASE/doom-$OPT-z$ZKB$TAG${EMU:+-emu}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
BIG=""; [ -n "$BIGRAM" ] && BIG="-e s/0x0003FFFF/0x003FFFFF/"   # measuring variant: 4 MB of RAM so a build that does not fit can still be sized and run
sed $BIG -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-8192}/" -e "s/^MEMORY/--heap_size=${HEAP:-2048}\n\nMEMORY/" "$G/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
[ -n "$EMU" ] && EMUF="-DFOX_EMU"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$SRC -I$HERE -I$HERE/include -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -w -ffunction-sections -fdata-sections $EMUF \
 -DFOX_DIRECT -DFOX_NOWIPE -DFOX_ZONE_STATIC=$((ZKB*1024)) -DFOX_WAD_MEM -DNORMALUNIX -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -include $HERE/fox_target.h $X"
. "$DOOM/srclist.sh"
for f in $SRCS; do "$CC/bin/tiarmclang" $CF -c "$SRC/$f.c" -o "$f.o" & done; wait
"$CC/bin/tiarmclang" $CF -c "$HERE/w_file_mem.c" -o w_file_mem.o
"$CC/bin/tiarmclang" $CF -c "$HERE/dg_mspm33.c" -o dg_mspm33.o
"$CC/bin/tiarmclang" $CF -c "$HERE/main.c" -o main.o
[ -z "$EMU" ] && { "$CC/bin/tiarmclang" $CF -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
  "$CC/bin/tiarmclang" $CF -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o; }
[ -n "$EMU" ] && "$CC/bin/tiarmclang" $CF -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o startup.o
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,doom.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o doom.out
echo "built $O/doom.out"
"$CC/bin/tiarmobjdump" -h doom.out | awk '$2 ~ /^\./ && $3!="00000000" {printf "%-14s %8d bytes  at %s\n",$2,strtonum("0x"$3),$4}' 2>/dev/null || "$CC/bin/tiarmobjdump" -h doom.out
