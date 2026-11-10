#!/bin/bash
# Build uLisp for the LP-MSPM33C321A (MSPM33C321A, Cortex-M33, 160 MHz, 256 kB RAM) with TI Arm Clang and the MSPM33 SDK DriverLib, no RTOS.
# BUILD ONLY: never touches a probe or a board.
#   ports/ulisp/boards/mspm33c321a/build.sh        -> $OUT/ulisp.out, ulisp.map, build.log, size.txt (OUT default ./out)
# Env: WORKSPACE (objects of 8 bytes, default 2800 = the 32 kB MSPM0 builds), OPT (default -Os), AUTOEXEC (Lisp text run 500 ms after reset), STACK (bytes, default 16384), SYSCONFIG_DIR.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"; UL="$REPO/ports/ulisp"
OPT="${OPT:--Os}"; WS="${WORKSPACE:-2800}"; STACK="${STACK:-16384}"
TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
CLANG="$CC/bin/tiarmclang"
SYSCFG_TOOL=""; for d in "${SYSCONFIG_DIR:-}" /Applications/ti/sysconfig_1.28.0 /Volumes/Seagate/ti/sysconfig_1.28.0; do [ -x "$d/sysconfig_cli.sh" ] && SYSCFG_TOOL="$d/sysconfig_cli.sh" && break; done
[ -n "$SYSCFG_TOOL" ] || { echo "SysConfig 1.28.x not found: set SYSCONFIG_DIR" >&2; exit 2; }
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
OUT="${OUT:-$PWD/out}"; B="$OUT"; G="$OUT/gen"; rm -rf "$B/o" "$B/src"; mkdir -p "$B/o" "$G"
if [ ! -f "$G/ti_msp_dl_config.c" ] || ! cmp -s "$HERE/ulisp.syscfg" "$G/ulisp.syscfg"; then
  rm -rf "$G"; mkdir -p "$G"; cp "$HERE/ulisp.syscfg" "$G/"
  "$SYSCFG_TOOL" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$HERE/ulisp.syscfg" > "$G/sysconfig.log" 2>&1 || { echo "SYSCONFIG FAILED"; tail -8 "$G/sysconfig.log"; exit 1; }
fi
FRL_STACKDIFF=${STACKDIFF:-1200} FRL_ENDSTACK=__stack python3 "$UL/patches/apply.py" "$B/src" > /dev/null || exit 1
BD=(); if [ -n "${AUTOEXEC:-}" ]; then python3 -c 'import sys,json; print("#define FRL_AUTOEXEC " + json.dumps(sys.argv[1] + "\r"))' "$AUTOEXEC" > "$B/autoexec.h"; BD=(-DFRL_LED_OUT); else rm -f "$B/autoexec.h"; fi
COMMON=(-mthumb $ARCH "@$G/device.opt" -I"$G" -I"$UL/shim" -I"$B" "-I$SDK/source" "-I$SDK/source/third_party/CMSIS/Core/Include" -gdwarf-3 -w)
CXXF=(-include "$HERE/compat.h" -x c++ -std=gnu++17 -fno-exceptions -fno-rtti -DFRL_MSPM0 -DFRL_WORKSPACE="$WS")
cd "$B"; : > build.log; ok=1
cc1() { echo "$CLANG $*" >> build.log; "$CLANG" "$@" >> build.log 2>&1 || ok=0; }
cc1 -O2 "${COMMON[@]}" -c "$G/ti_msp_dl_config.c" -o o/ti_msp_dl_config.o
cc1 -O2 "${COMMON[@]}" -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o o/startup.o
cc1 -O2 "${COMMON[@]}" ${BD[@]+"${BD[@]}"} -c "$HERE/board.c" -o o/board.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$B/src/ulisp-arm.cpp" -o o/ulisp.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$UL/shim/arduino_shim.cpp" -o o/shim.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$HERE/mainthread.cpp" -o o/mainthread.o
sed -E "s/^--stack_size=[0-9]+/--stack_size=$STACK/" "$G/device_linker.cmd" > linker.cmd
if [ $ok = 1 ]; then
  "$CLANG" $ARCH -Wl,-u,_c_int00 o/*.o -Wl,--unused_section_elimination=on -l"$G/device.cmd.genlibs" "-L$SDK/source" ./linker.cmd \
    -Wl,-m,ulisp.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o ulisp.out >> build.log 2>&1 || ok=0
fi
if [ $ok = 0 ]; then echo "BUILD FAILED"; grep -iE "error|undefined|overflow|does not fit" build.log | head -12; exit 1; fi
grep -E '^ +(BCR|BSL)_CONFIG ' ulisp.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}' || exit 1
sz() { python3 -c "import re,sys;m=open('ulisp.map').read();r=re.search(r'^\s+%s\s+(\w+)\s+(\w+)\s+(\w+)'%sys.argv[1],m,re.M);print(int(r.group(2),16),int(r.group(3),16))" "$1"; }
read RAMLEN RAMUSED <<< "$(sz SRAM)"; read FLLEN FLUSED <<< "$(sz FLASH)"
echo "mspm33c321a flash_used=$FLUSED flash_len=$FLLEN ram_used=$RAMUSED ram_len=$RAMLEN stack_bytes=$STACK static_ram=$((RAMUSED-STACK)) workspace_objects=$WS workspace_bytes=$((WS*8)) opt=$OPT" | tee size.txt
rm -rf o; echo "built $B/ulisp.out"
