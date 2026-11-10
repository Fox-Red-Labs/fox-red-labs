#!/bin/bash
# Build uLisp for the LAUNCHXL-F2800137 (C28x, 120 MHz, 36 kB RAM) with cl2000 25.11.1.LTS and C2000Ware 26.02. BUILD ONLY: never touches a probe.
#   ports/ulisp/boards/f2800137/build.sh   -> $OUT/ulisp.out, ulisp.map, build.log (OUT default ./out)
# Env: WORKSPACE (objects, default 2000), OPT (default 2), STACKWORDS (default 0x1800), AUTOEXEC (Lisp text run 500 ms after reset), STACKDIFF.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"; UL="$REPO/ports/ulisp"
OPT="${OPT:-2}"; WS="${WORKSPACE:-2000}"
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
CGT=${CGT:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS}
OUT="${OUT:-$PWD/out}"; rm -rf "$OUT/o"; mkdir -p "$OUT/o"; cd "$OUT"
FRL_STACKDIFF=${STACKDIFF:-320} python3 "$UL/patches/apply.py" "$OUT/src" > /dev/null || exit 1
# The C28x stack grows up: point the sketch's depth check at the linker's stack-end symbol and measure from the address of a local (cl2000 has no named register variables).
python3 - "$OUT/src/ulisp-arm.cpp" <<'PY' || exit 1
import sys
p = sys.argv[1]; s = open(p).read()
def once(s, a, b):
    assert s.count(a) == 1, a
    return s.replace(a, b)
s = once(s, "#define ENDSTACK end\n#endif", "#define ENDSTACK __TI_STACK_END\n#endif")
s = once(s, 'register int *sp asm ("sp");', 'int spmark; int *sp = &spmark;')
s = once(s, "((uint32_t)sp - (uint32_t)&ENDSTACK) < STACKDIFF", "((uint32_t)&ENDSTACK - (uint32_t)sp) < STACKDIFF")
# C28x int is 16 bits: radix-40 symbol packing and the packed-string builders need 32-bit arithmetic.
s = once(s, "  int x = 0, j = 0;\n  for (int i=0; i<6; i++) {\n    x = x * 40", "  uint32_t x = 0; int j = 0;\n  for (int i=0; i<6; i++) {\n    x = x * 40")
s = once(s, "(*tail)->chars |= ch<<16; return;", "(*tail)->chars |= (uint32_t)ch<<16; return;")
s = once(s, "(*tail)->chars |= ch<<8; return;", "(*tail)->chars |= (uint32_t)ch<<8; return;")
s = once(s, "cell->chars = ch<<24; *tail = cell;", "cell->chars = (uint32_t)ch<<24; *tail = cell;")
# The sketch assumes a 32-bit int (integers, string packing, number printing): widen every int to long (32 bits on the C28x), and uintptr_t to 32 bits.
import re
s = s.replace("unsigned int", "unsigned long")
s = re.sub(r"\bint\b(?!8_t|16_t|32_t|64_t)", "long", s)
s = s.replace("uintptr_t", "uint32_t")
# sizeof counts 16-bit words here; the sketch means a 4-byte word
s = s.replace("sizeof(long)", "4")
open(p, "w").write(s)
PY
if [ -n "${AUTOEXEC:-}" ]; then python3 -c 'import sys,json; print("#define FRL_AUTOEXEC " + json.dumps(sys.argv[1] + "\r"))' "$AUTOEXEC" > autoexec.h; else rm -f autoexec.h; fi
# The sketch is compiled WITHOUT the C2000Ware include paths: on a case-insensitive disk its #include <SPI.h> would find driverlib's spi.h.
CF="-v28 -ml -mt --float_support=fpu32 --abi=eabi --opt_level=$OPT --gen_func_subsections=on --define=CPU1 --define=_FLASH --diag_suppress=10063 -I$CGT/include"
CS="-I$UL/shim -I$OUT"; mkdir -p hal; cp "$UL/shim/frl_hal.h" hal/
CW="-I$OUT/hal --define=_LAUNCHXL_F2800137 --define=F280013x=1 -I$C2K -I$C2K/device_support/f280013x/common/include -I$C2K/driverlib/f280013x/driverlib"
: > build.log; ok=1
cc() { echo "cl2000 $*" >> build.log; "$CGT/bin/cl2000" $CF "$@" >> build.log 2>&1 || ok=0; }
cc $CS --preinclude="$HERE/compat.h" --define=FRL_MSPM0 --define=FRL_WORKSPACE=$WS -c "$OUT/src/ulisp-arm.cpp" --output_file=o/ulisp.obj
cc $CS --preinclude="$HERE/compat.h" --define=FRL_MSPM0 -c "$UL/shim/arduino_shim.cpp" --output_file=o/shim.obj
cc $CS --preinclude="$HERE/compat.h" -c "$HERE/mainthread.cpp" --output_file=o/mainthread.obj
cc $CW -c "$HERE/board.c" --output_file=o/board.obj
cc $CW -c "$C2K/device_support/f280013x/common/source/device.c" --output_file=o/device.obj
cc $CW -c "$C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm" --output_file=o/codestart.obj
[ $ok = 1 ] && "$CGT/bin/cl2000" $CF -z --entry_point code_start --stack_size=${STACKWORDS:-0x1800} --heap_size=0x100 -i"$CGT/lib" -i"$C2K" --reread_libs --rom_model \
  -m ulisp.map -o ulisp.out o/*.obj "$HERE/f2800137_ulisp.cmd" -l"$C2K/driverlib/f280013x/driverlib/ccs/Release/driverlib.lib" -llibc.a >> build.log 2>&1 || ok=0
if [ $ok = 0 ]; then echo "BUILD FAILED"; grep -E "error|undefined|overflow|cannot|unresolved" build.log | head -12; exit 1; fi
echo "built $OUT/ulisp.out workspace_objects=$WS"
