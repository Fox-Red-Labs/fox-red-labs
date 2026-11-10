#!/bin/bash
# Build uLisp for the LP-EM-CC2340R5 (CC2340R52, Cortex-M0+, 48 MHz, 36 kB RAM) with TI Arm Clang, SimpleLink Low Power F3 SDK, NoRTOS.
# BUILD ONLY: never touches a probe or a board.
#   ports/ulisp/boards/cc2340r5/build.sh        -> $OUT/ulisp.out, ulisp.hex, ulisp.map, build.log, ccfg.txt, size.txt (OUT default ./out)
# Env: WORKSPACE (objects of 8 bytes, default 2800 = the 32 kB MSPM0 builds), OPT (default -Os), AUTOEXEC (Lisp text run 500 ms after reset), SYSCONFIG_DIR.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"; UL="$REPO/ports/ulisp"
OPT="${OPT:--Os}"; WS="${WORKSPACE:-2800}"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
SYSCFG_TOOL=""; for d in "${SYSCONFIG_DIR:-}" /Applications/ti/sysconfig_1.26.3 /Volumes/Seagate/ti/sysconfig_1.26.3; do [ -x "$d/sysconfig_cli.sh" ] && SYSCFG_TOOL="$d/sysconfig_cli.sh" && break; done
[ -n "$SYSCFG_TOOL" ] || { echo "SysConfig 1.26.3 not found: set SYSCONFIG_DIR" >&2; exit 2; }
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; CLANG="$CGT/bin/tiarmclang"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft"; BENCH="$REPO/cc2340r5/bench"
OUT="${OUT:-$PWD/out}"; B="$OUT"; G="$OUT/gen"; rm -rf "$B/o" "$B/src"; mkdir -p "$B/o" "$G"
if [ ! -f "$G/ti_drivers_config.c" ] || ! cmp -s "$HERE/ulisp.syscfg" "$G/ulisp.syscfg"; then
  rm -rf "$G"; mkdir -p "$G"; cp "$HERE/ulisp.syscfg" "$G/"
  "$SYSCFG_TOOL" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$HERE/ulisp.syscfg" > "$G/sysconfig.log" 2>&1 || { echo "SYSCONFIG FAILED"; tail -8 "$G/sysconfig.log"; exit 1; }
fi
FRL_STACKDIFF=${STACKDIFF:-1200} FRL_ENDSTACK=__stack python3 "$UL/patches/apply.py" "$B/src" > /dev/null || exit 1
BD=(); if [ -n "${AUTOEXEC:-}" ]; then python3 -c 'import sys,json; print("#define FRL_AUTOEXEC " + json.dumps(sys.argv[1] + "\r"))' "$AUTOEXEC" > "$B/autoexec.h"; BD=(-DFRL_LED_OUT); else rm -f "$B/autoexec.h"; fi
COMMON=(-mthumb $ARCH "@$G/ti_utils_build_compiler.opt" -I"$G" -I"$UL/shim" -I"$B" "-I$SDK/source" "-I$SDK/kernel/nortos" "-I$SDK/kernel/nortos/posix" -gdwarf-3 -w)
CXXF=(-include "$HERE/compat.h" -x c++ -std=gnu++17 -fno-exceptions -fno-rtti -DFRL_MSPM0 -DFRL_WORKSPACE="$WS")
cd "$B"; : > build.log; ok=1
cc1() { echo "$CLANG $*" >> build.log; "$CLANG" "$@" >> build.log 2>&1 || ok=0; }
for f in "$BENCH/board/main_nortos.c" "$G"/ti_*.c; do [ -f "$f" ] && cc1 -O2 "${COMMON[@]}" -c "$f" -o "o/$(basename "${f%.c}").o"; done
cc1 -O2 "${COMMON[@]}" ${BD[@]+"${BD[@]}"} -c "$HERE/board.c" -o o/board.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$B/src/ulisp-arm.cpp" -o o/ulisp.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$UL/shim/arduino_shim.cpp" -o o/shim.o
cc1 "$OPT" "${COMMON[@]}" "${CXXF[@]}" -c "$HERE/mainthread.cpp" -o o/mainthread.o
link() {
  sed -E "s/^--stack_size=[0-9]+/--stack_size=$1/; s/^--heap_size=[0-9]+/--heap_size=0/" "$BENCH/board/lpf3_nortos.cmd" > linker.cmd
  "$CLANG" $ARCH -Wl,-u,_c_int00 o/*.o -Wl,--unused_section_elimination=on "-L$SDK/source" "-L$SDK/kernel/nortos" "-Wl,--search_path=$G" \
    "$G/ti_utils_build_linker.cmd.genlibs" ./linker.cmd -Wl,-m,ulisp.map -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o ulisp.out >> build.log 2>&1
}
sz() { python3 -c "import re,sys;m=open('ulisp.map').read();r=re.search(r'^\s+%s\s+(\w+)\s+(\w+)\s+(\w+)'%sys.argv[1],m,re.M);print(int(r.group(2),16),int(r.group(3),16))" "$1"; }
STACK=0
if [ $ok = 1 ]; then
  rm -f ulisp.out ulisp.map; link 1024
  if [ -f ulisp.out ]; then read RAMLEN RAMUSED <<< "$(sz SRAM)"; STACK=$(( (RAMLEN - (RAMUSED - 1024)) / 8 * 8 )); else ok=0; fi
  if [ $ok = 1 ]; then rm -f ulisp.out ulisp.map; for i in $(seq 40); do link "$STACK" && break; STACK=$((STACK-64)); done; [ -f ulisp.out ] || ok=0; fi
fi
[ $ok = 1 ] && { "$SDK/tools/common/crc_tool/crc_tool" patch-image --elf ulisp.out --symbol-prefix ti_utils_build_GenMap_sym_CRC --output ulisp.out >> build.log 2>&1 || ok=0; }
[ $ok = 1 ] && { "$CGT/bin/tiarmobjcopy" -O ihex ulisp.out ulisp.hex >> build.log 2>&1 || ok=0; }
if [ $ok = 0 ]; then echo "BUILD FAILED (stack $STACK)"; grep -iE "error|undefined|overflow|does not fit" build.log | head -12; exit 1; fi
cfg="$G/ti_devices_config.c"
for p in allowDebugPort allowEnergyTrace allowFlashVerify allowFlashProgram allowChipErase allowToolsClientMode allowReturnToFactory; do
  grep -qE "\.$p[[:space:]]*=[[:space:]]*CCFG_PERMISSION_ALLOW" "$cfg" || { echo "CCFG: $p is not ALLOW"; exit 1; }; done
echo "cc23 CCFG: all permission fields = CCFG_PERMISSION_ALLOW" > ccfg.txt
read RAMLEN RAMUSED <<< "$(sz SRAM)"; read FLLEN FLUSED <<< "$(sz FLASH)"
echo "cc2340r5 flash_used=$FLUSED flash_len=$FLLEN ram_used=$RAMUSED ram_len=$RAMLEN stack_bytes=$STACK static_ram=$((RAMUSED-STACK)) workspace_objects=$WS workspace_bytes=$((WS*8)) opt=$OPT" | tee size.txt
rm -rf o; echo "built $B/ulisp.out"
