#!/bin/bash
# One command: bash build_target.sh <game> <board> [out-dir]
#   game  = crate-confusion | snake | tileworld
#   board = mspm0g3507 | mspm0g3519 | mspm0g5187 | mspm33c321a
# Cross-compiles the Arduboy2 layer plus the game with TI Arm Clang, using the SDK's startup file and linker
# script for the chip (so the memory map is the real one), with a null display (see target/main_null.cpp).
# Prints flash and RAM. A link error about a region means the build does not fit.
# SDK paths default to the TI install folders on macOS; set MSPM0_SDK, MSPM33_SDK, TC to use others.
set -e
cd "$(dirname "$0")"
G=$1; B=$2; [ -n "$G" ] && [ -n "$B" ] || { echo "usage: build_target.sh <game> <board> [out-dir]"; exit 1; }
O=${3:-out/target}/$B/$G; mkdir -p "$O"
M0=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}
M33=${MSPM33_SDK:-/Applications/ti/mspm33_sdk_1_04_00_00}
TC=${TC:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
GD=../../third-party/arduboy-games/$G
case $B in
  mspm0g3507) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G3507__; ST=startup_mspm0g350x_ticlang.c; LD=mspm0g3507.cmd ;;
  mspm0g3519) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G3519__; ST=startup_mspm0g351x_ticlang.c; LD=mspm0g3519.cmd ;;
  mspm0g5187) SDK=$M0; FAM=m0p; ARCH="-mcpu=cortex-m0plus -mfloat-abi=soft"; DEF=__MSPM0G5187__; ST=startup_mspm0g518x_ticlang.c; LD=mspm0g5187.cmd ;;
  mspm33c321a) SDK=$M33; FAM=m33; ARCH="-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16"; DEF=__MSPM33C321A__; ST=startup_mspm33c321x_ticlang.c; LD=mspm33c321a.cmd ;;
  *) echo "unknown board $B"; exit 1 ;;
esac
CC="$TC/bin/tiarmclang"
EXTRA=""; [ "$G" = crate-confusion ] && EXTRA="-DFRL_RENAME_MODE_T"
[ -n "$FRL_1BPP" ] && EXTRA="$EXTRA -DFRL_TARGET_1BPP"
CF="$ARCH -mthumb -Os -w -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics -D$DEF -Iinclude -I$GD -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
CFC="$CF -x c++ -std=gnu++17"
OBJS=""
for f in $(ls $GD/*.ino); do
  python3 tools/ino_protos.py $f $O/$(basename $f).cpp
  $CC $CFC -include tools/ino_prelude.h $EXTRA -c $O/$(basename $f).cpp -o $O/$(basename $f).o; OBJS="$OBJS $O/$(basename $f).o"
done
for f in $(ls $GD/*.cpp 2>/dev/null); do
  $CC $CFC -include tools/ino_prelude.h $EXTRA -c $f -o $O/$(basename $f).o; OBJS="$OBJS $O/$(basename $f).o"
done
for f in src/arduboy2.cpp src/frl_core.cpp target/main_null.cpp; do
  $CC $CFC $EXTRA -c $f -o $O/$(basename $f).o; OBJS="$OBJS $O/$(basename $f).o"
done
$CC $ARCH -mthumb -Os -w -D$DEF -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -c $SDK/source/ti/devices/msp/$FAM/startup_system_files/ticlang/$ST -o $O/startup.o
LDF=$SDK/source/ti/devices/msp/$FAM/linker_files/ticlang
$CC $ARCH -mthumb $O/startup.o $OBJS -o $O/$G.out -Wl,-u,_c_int00 -Wl,--stack_size=${STACK:-2048} -Wl,--heap_size=0 \
  -Wl,-m,$O/$G.map -Wl,--rom_model -L$LDF -Wl,-l$LD -L$TC/lib -llibc.a 2>&1 | tee $O/link.log | grep -i "error\|overflow\|will not fit" || true
if [ -f $O/$G.out ]; then
  "$TC/bin/tiarmsize" -A $O/$G.out > $O/size.txt 2>&1 || true
  python3 tools/size_report.py $O/size.txt $B $G
else
  echo "$B $G: no image (link failed; see $O/link.log)"
fi
