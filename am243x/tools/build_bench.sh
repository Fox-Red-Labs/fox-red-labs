#!/bin/bash
# Build one bare-metal bench program for the LP-AM243 (R5FSS0-0, nortos) from a main.c and extra sources, with TI Arm Clang 4.0.4
# and the SDK's SysConfig output for TI's own coremark_benchmark example (800 MHz, UART0 console).
#   build_bench.sh <name> <opt: o3 o2 os oz> <mem> <main.c> [extra compile flags...]      EXTRA_SRC="a.c b.c" adds sources
#   mem: ocram (TI's linker placement: code and stack in OCRAM, .data in TCMA) or tcm (the code and constants of EXTRA_TCM_OBJ
#        files, default core_*, in TCMA and the main stack in TCMB)
# Output: $OUT/<name>-<opt>-<mem>/<name>.out (+ .map). Shared bench sources live in am263p/bench. One-time setup: copy the SDK coremark_benchmark generated/ folder to $OUT/gen.
set -u
NAME="$1"; OPT="$2"; MEM="$3"; MAIN="$4"; shift 4
HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/am243x}"
SDK=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33; CG=${CG:-/Applications/ti/ti-cgt-armllvm_release}
G="$OUT/gen"; O="$OUT/$NAME-$OPT-$MEM"; rm -rf "$O" "$O.log"; mkdir -p "$O/obj"
[ -f "$G/linker.cmd" ] || { echo "copy the SDK coremark_benchmark generated/ folder to $OUT/gen first"; exit 1; }
cp "$G/linker.cmd" "$O/linker.cmd"
if [ "$MEM" = tcm ]; then
  python3 - "$O/linker.cmd" "${EXTRA_TCM_OBJ:-core_}" <<'PY'
import sys,re
p,pat=sys.argv[1],sys.argv[2]
t=open(p).read()
sel=" ".join(f"*{x}*.obj(.text*) *{x}*.obj(.rodata*)" for x in pat.split(","))
t=t.replace("    GROUP  :   {\n    .text.hwi", f"    .text.bench : {{ {sel} }} > R5F_TCMA , align(8)\n    GROUP  :   {{\n    .text.hwi",1)
# main stack into TCMB: split .stack out of the OCRAM memory-segments group
t=t.replace("    .stack : {\n    } align(8)\n","",1)
t=t.replace("    .bss.user_shared_mem","    .stack : {\n    } > R5F_TCMB0 , align(8)\n    .bss.user_shared_mem",1)
open(p,"w").write(t)
PY
fi
[ -n "${LINK_PATCH:-}" ] && python3 "$LINK_PATCH" "$O/linker.cmd"
CF="-mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb -Wall -Werror -g -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -${OPT/o/O} ${OPTX:-}"
DEF="-DSOC_AM243X -DOS_NORTOS -DCPU_HZ=800000000u"
INC="${EXTRA_INC:-} -I$CG/include/c -I$SDK/source -I$SDK/source/kernel/dpl/ -I$G -I$REPO/am263p/bench/coremark -I$REPO/third-party/coremark"
OBJS=""
for f in "$MAIN" ${EXTRA_SRC:-} "$G"/ti_drivers_config.c "$G"/ti_drivers_open_close.c "$G"/ti_board_config.c "$G"/ti_board_open_close.c "$G"/ti_dpl_config.c "$G"/ti_pinmux_config.c "$G"/ti_power_clock_config.c; do
  o="$O/obj/$(basename "${f%.c}").obj"; OBJS="$OBJS $o"
  # warnings stay on, but EEMBC and generated sources are not edited: -Werror only for our own files
  W="$CF"; case "$f" in */third-party/*|*/dhrystone/src/*|*/h264bsd/src/*|"$G"/*) W="${CF/-Werror/}";; esac
  X=""; case "$f" in
    */core_main.c) X="-Dmain=coremark_main";;
    */dhry_1.c) X="-std=gnu89 -w -DTIME -include string.h -Dmain=dhry_main -Dprintf=dh_printf -Dscanf=dh_scanf -Dmalloc=dh_malloc";;
    */h264bsd/src/*.c) X="-std=gnu99 -w -Dmalloc=h_malloc -Dfree=h_free -include stdlib.h";;
    */dhry_2.c) X="-std=gnu89 -w -DTIME -include string.h";;
  esac
  "$CG/bin/tiarmclang" -c $W $INC $DEF $X $* -o "$o" "$f" >> "$O.log" 2>&1 || { echo "compile failed: $O.log"; exit 1; }
done
LP="-Wl,-i$G -Wl,-i$SDK/source/kernel/nortos/lib -Wl,-i$SDK/source/drivers/lib -Wl,-i$SDK/source/board/lib -Wl,-i$SDK/source/mathlib/lib -Wl,-i$CG/lib"
LIBS="-lnortos.am243x.r5f.ti-arm-clang.release.lib -ldrivers.am243x.r5f.ti-arm-clang.nortos.release.lib -lmathlib.am243x.r5f.ti-arm-clang.nortos.release.lib -lboard.am243x.r5f.ti-arm-clang.nortos.release.lib -llibc.a -llibsysbm.a"
"$CG/bin/tiarmclang" -Wl,--diag_suppress=10063 -Wl,--ram_model -Wl,--reread_libs -Wl,--zero_init=on -Wl,--use_memset=fast -Wl,--use_memcpy=fast \
  $LP -Wl,-m="$O/$NAME.map" -o "$O/$NAME.out" $OBJS $LIBS "$O/linker.cmd" >> "$O.log" 2>&1 || { echo "link failed: $O.log"; exit 1; }
echo "built $O/$NAME.out"
