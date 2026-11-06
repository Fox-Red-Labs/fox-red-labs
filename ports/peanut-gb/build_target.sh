#!/bin/bash
# One command: bash build_target.sh <board> [rom]
#   board = mspm0g3507 | mspm0g3519 | mspm0g5187 | mspm33c321a
#   rom   = link Tobu Tobu Girl (roms/tobu.gb, 256 KB) into flash; acid = link dmg-acid2 (roms/dmg-acid2.gb, 32 KB);
#           without either the ROM is read from a fixed address (code and RAM only)
# Env: PGB_SCRIPT=scripts/tobu.txt (scripted buttons in the null entry point, for speed/emu_pgb.py), TAG=name (output folder suffix), FOPT=-O3 (optimisation of the front end and Peanut-GB; default -Os), PGB_FLAGS="-D..." (front-end switches, see src/pgb_frontend.c).
# Cross-compiles the Peanut-GB front end (unmodified peanut_gb.h) with TI Arm Clang and the SDK startup file and linker script,
# null display. Prints flash and RAM. A link error about a region means the build does not fit.
set -e
cd "$(dirname "$0")"
PD=${PGB_PATCHED:-out/peanut-gb-patched}   # PGB_SKIP_PATCH=0002 with PGB_PATCHED=out/pg-no0002 builds without the speed patch
bash patches/apply.sh $PD
B=$1; V=$2; [ -n "$B" ] || { echo "usage: build_target.sh <board> [rom]"; exit 1; }
O=out/target/$B${V:+-$V}${TAG:+-$TAG}; mkdir -p "$O"
M0=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}
M33=${MSPM33_SDK:-/Applications/ti/mspm33_sdk_1_04_00_00}
TC=${TC:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
A=../arduboy
case $B in
  mspm0g3507) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G3507__; ST=startup_mspm0g350x_ticlang.c; LD=mspm0g3507.cmd ;;
  mspm0g3519) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G3519__; ST=startup_mspm0g351x_ticlang.c; LD=mspm0g3519.cmd ;;
  mspm0g5187) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G5187__; ST=startup_mspm0g518x_ticlang.c; LD=mspm0g5187.cmd ;;
  mspm33c321a) SDK=$M33; FAM=m33; ARCH="-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16"; DEF=__MSPM33C321A__; ST=startup_mspm33c321x_ticlang.c; LD=mspm33c321a.cmd ;;
  *) echo "unknown board $B"; exit 1 ;;
esac
CC="$TC/bin/tiarmclang"
X="-DPGB_X"; case "$V" in rom) X="-DPGB_ROM_EMBEDDED"; ROMF=roms/tobu.gb;; acid) X="-DPGB_ROM_EMBEDDED"; ROMF=roms/dmg-acid2.gb;; esac
[ -n "$FRL_1BPP" ] && X="$X -DFRL_TARGET_1BPP"
if [ -n "$PGB_SCRIPT" ]; then   # scripted buttons for the instruction counter: scripts/tobu.txt lines "<frame> <KEYS>" become a table
  python3 - "$PGB_SCRIPT" "$O/script.h" <<'PY'
import sys
K = {"UP": 1, "DOWN": 2, "LEFT": 4, "RIGHT": 8, "A": 16, "B": 32}
rows = []
for l in open(sys.argv[1]):
    if l.startswith("#") or not l.strip(): continue
    f, k = (l.split() + ["NONE"])[:2]
    rows.append((int(f), sum(K.get(t, 0) for t in k.split("+"))))
open(sys.argv[2], "w").write("static const struct { unsigned frame; int keys; } pgb_script[] = {" + ",".join("{%d,%d}" % r for r in rows) + "};\n")
PY
  X="$X -DPGB_NULL_SCRIPT -I$O"
fi
CF="$ARCH -mthumb -Os -w -ffunction-sections -fdata-sections -D$DEF -I$A/include -Isrc -I$PD -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
CXXF="$CF -x c++ -std=gnu++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics"
OBJS=""
$CC $CF ${FOPT:--Os} $PGB_FLAGS -std=gnu11 $X -c src/pgb_frontend.c -o $O/pgb_frontend.o; OBJS="$OBJS $O/pgb_frontend.o"
for f in $A/src/frl_core.cpp target/main_pgb.cpp; do $CC $CXXF $X -c $f -o $O/$(basename $f).o; OBJS="$OBJS $O/$(basename $f).o"; done
if [ -n "$ROMF" ]; then
  python3 $A/tools/bin2c.py $ROMF $O/rom_data.c frl_rom_data frl_rom_len
  $CC $CF -std=gnu11 -c $O/rom_data.c -o $O/rom_data.o; OBJS="$OBJS $O/rom_data.o"
fi
$CC $ARCH -mthumb -Os -w -D$DEF -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -c $SDK/source/ti/devices/msp/$FAM/startup_system_files/ticlang/$ST -o $O/startup.o
LDF=$SDK/source/ti/devices/msp/$FAM/linker_files/ticlang
$CC $ARCH -mthumb $O/startup.o $OBJS -o $O/pgb.out -Wl,-u,_c_int00 -Wl,--heap_size=0 \
  -Wl,-m,$O/pgb.map -Wl,--rom_model -L$LDF -Wl,-l$LD -L$TC/lib -llibc.a 2>&1 | tee $O/link.log | grep -i "error\|overflow\|will not fit" || true
if [ -f $O/pgb.out ]; then
  "$TC/bin/tiarmsize" -A $O/pgb.out > $O/size.txt 2>&1 || true
  python3 $A/tools/size_report.py $O/size.txt $B peanut-gb${V:+-$V}
else
  echo "$B peanut-gb${V:+-$V}: no image (link failed; see $O/link.log)"
fi
