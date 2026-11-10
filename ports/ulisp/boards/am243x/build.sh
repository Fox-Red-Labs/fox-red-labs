#!/bin/bash
# Build uLisp for the LP-AM243 (R5FSS0-0, nortos, 800 MHz) with TI Arm Clang 4.0.4 and the MCU+ SDK. BUILD ONLY: never touches a probe or a board.
#   ports/ulisp/boards/am243x/build.sh   -> $OUT/ulisp-am243x/ulisp.out, ulisp.map, build.log, size.txt   (OUT default ./out/am243x)
# Needs the one-time setup of am243x/tools/build_bench.sh: $OUT/gen holds the SDK coremark_benchmark generated/ folder.
# Env: WORKSPACE (objects of 8 bytes, default 2800 = the 32 kB MSPM0 builds), OPT (default -Os).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"; UL="$REPO/ports/ulisp"
OPT="${OPT:--Os}"; WS="${WORKSPACE:-2800}"
SDK=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33; CG=${CG:-/Applications/ti/ti-cgt-armllvm_release}
OUT="${OUT:-$PWD/out/am243x}"; G="$OUT/gen"; B="$OUT/ulisp-am243x"; rm -rf "$B"; mkdir -p "$B/o"
[ -f "$G/linker.cmd" ] || { echo "copy the SDK coremark_benchmark generated/ folder to $G first"; exit 1; }
cd "$B"; : > build.log
FRL_STACKDIFF=${STACKDIFF:-1500} FRL_ENDSTACK=__stack_floor python3 "$UL/patches/apply.py" "$B/src" > /dev/null || exit 1
cp "$G/linker.cmd" linker.cmd; python3 "$HERE/link_patch.py" linker.cmd || exit 1
ARCH=(-mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb)
DEF=(-DSOC_AM243X -DOS_NORTOS -DCPU_HZ=800000000u)
INC=(-I"$CG/include/c" -I"$SDK/source" -I"$SDK/source/kernel/dpl/" -I"$G" -I"$UL/shim" -I"$B")
ok=1
cc1() { echo "$*" >> build.log; "$CG/bin/tiarmclang" "$@" >> build.log 2>&1 || ok=0; }
for f in "$G"/ti_*.c; do cc1 -c "${ARCH[@]}" -O2 -g -w "${INC[@]}" "${DEF[@]}" -o "o/$(basename "${f%.c}").o" "$f"; done
cc1 -c "${ARCH[@]}" -O2 -g -w "${INC[@]}" "${DEF[@]}" -o o/board.o "$HERE/board.c"
CXXF=(-include "$HERE/compat.h" -x c++ -std=gnu++17 -fno-exceptions -fno-rtti -DFRL_MSPM0 -DFRL_WORKSPACE="$WS")
cc1 -c "${ARCH[@]}" "$OPT" -g -w "${INC[@]}" "${DEF[@]}" "${CXXF[@]}" -o o/ulisp.o "$B/src/ulisp-arm.cpp"
cc1 -c "${ARCH[@]}" "$OPT" -g -w "${INC[@]}" "${DEF[@]}" "${CXXF[@]}" -o o/shim.o "$UL/shim/arduino_shim.cpp"
cc1 -c "${ARCH[@]}" "$OPT" -g -w "${INC[@]}" "${DEF[@]}" "${CXXF[@]}" -o o/main.o "$HERE/main.cpp"
[ $ok = 1 ] || { echo "compile failed: $B/build.log"; grep -m8 -iE "error" build.log; exit 1; }
LP="-Wl,-i$G -Wl,-i$SDK/source/kernel/nortos/lib -Wl,-i$SDK/source/drivers/lib -Wl,-i$SDK/source/board/lib -Wl,-i$SDK/source/mathlib/lib -Wl,-i$CG/lib"
LIBS="-lnortos.am243x.r5f.ti-arm-clang.release.lib -ldrivers.am243x.r5f.ti-arm-clang.nortos.release.lib -lmathlib.am243x.r5f.ti-arm-clang.nortos.release.lib -lboard.am243x.r5f.ti-arm-clang.nortos.release.lib -llibc.a -llibsysbm.a"
"$CG/bin/tiarmclang" "${ARCH[@]}" -Wl,--diag_suppress=10063 -Wl,--ram_model -Wl,--reread_libs -Wl,--zero_init=on -Wl,--use_memset=fast -Wl,--use_memcpy=fast \
  $LP -Wl,-m=ulisp.map -o ulisp.out o/*.o $LIBS linker.cmd >> build.log 2>&1 || { echo "link failed: $B/build.log"; grep -m8 -iE "error|undefined" build.log; exit 1; }
"$CG/bin/tiarmsize" ulisp.out 2>/dev/null | tail -1 | tee size.txt
echo "built $B/ulisp.out"
