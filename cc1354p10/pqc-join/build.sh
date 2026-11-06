#!/bin/bash
# Build the post-quantum join image for the LP-CC1354P10-1 from the terminal. BUILD ONLY: never touches a probe or a board.
#   ./build.sh [-O3|-Os|-O2]      (default -O3) -> out/pqc-join-<opt>/image.out, image.map, build.log, ccfg.txt
# TI Arm Clang from CCStudio 21.0.1, SimpleLink CC13xx/CC26xx SDK 8.33, SysConfig 1.26.3: the same tools as cc1354p10/bench/build.sh.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
OPT="${1:--O3}"; NAME="pqc-join${OPT}${LOWRAM:+-lowram}"   # LOWRAM=1: ML-DSA-44 with -DMLD_CONFIG_REDUCE_RAM, image in out/pqc-join<opt>-lowram
BOARD=cc1354p10; FILTER=""
SDK=/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16
SYSCFG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
ARCH="-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16"
BB="$REPO/cc1354p10/bench/board"; LIB="$REPO/cc2340r5/bench/lib"
CCFG_KIND=cc13x4
. "$LIB/build-lib.sh" 2>/dev/null   # for gen() and ccfg_check(); it also makes out/images.csv, which we ignore
OUT="$HERE/out"; B="$OUT/$NAME"; rm -rf "$B"; mkdir -p "$B"
G=$(gen "$HERE/pqc.syscfg") || exit 1
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
BID="$BOARD-$NAME-$(date +%Y%m%d%H%M%S)"
COMMON=(-mthumb $ARCH "@$G/ti_utils_build_compiler.opt" -I"$G" -I"$LIB" "-I$SDK/source" "-I$SDK/kernel/nortos" "-I$SDK/kernel/nortos/posix" -gdwarf-3 -Wall)
MLF=(-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I"$ML" -I"$PJ")
MDF=(-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API ${LOWRAM:+-DMLD_CONFIG_REDUCE_RAM} -I"$MD" -I"$PJ")
: > "$B/build.log"; ok=1
cc1() { echo "$CLANG $*" >> "$B/build.log"; "$CLANG" "$@" >> "$B/build.log" 2>&1 || ok=0; }
for f in "$BB/main_nortos.c" "$G"/ti_*.c; do [ -f "$f" ] && cc1 -O2 "${COMMON[@]}" -c "$f" -o "$B/$(basename "${f%.c}").o"; done
cc1 -O2 "${COMMON[@]}" "${MLF[@]}" -I"$MD" -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"$OPT\"" -c "$HERE/pqc_board.c" -o "$B/pqc_board.o"
cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -c "$ML/mlkem_native.c" -o "$B/mlkem_native.o"
cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -c "$PJ/pqc_join.c" -o "$B/pqc_join.o"
cc1 "$OPT" "${COMMON[@]}" -c "$PJ/pqc_sym.c" -o "$B/pqc_sym.o"
cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$MD/mldsa_native.c" -o "$B/mldsa_native.o"
cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$PJ/pqc_sign.c" -o "$B/pqc_sign.o"
if [ $ok = 1 ]; then
	sed -E "s/^--stack_size=[0-9]+/--stack_size=${STACK:-65536}/; s/^--heap_size=[0-9]+/--heap_size=0/" "$BB/cc13x4_cc26x4_nortos.cmd" > "$B/linker.cmd"
	( cd "$B" && "$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o "-L$SDK/source" "-L$SDK/kernel/nortos" "-Wl,--search_path=$G" \
		"$G/ti_utils_build_linker.cmd.genlibs" ./linker.cmd "-Wl,-m,image.map" -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o image.out ) >> "$B/build.log" 2>&1 || ok=0
fi
[ $ok = 1 ] && "$CGT/bin/tiarmobjcopy" -O ihex "$B/image.out" "$B/image.hex" >> "$B/build.log" 2>&1
if [ $ok = 0 ]; then echo "BUILD FAILED"; grep -iE "error|undefined" "$B/build.log" | head -15; exit 1; fi
cc=$(ccfg_check "$G") || { echo "CCFG CHECK FAILED: $cc"; exit 1; }
echo "$cc" > "$B/ccfg.txt"; echo "$BID" > "$B/build-id.txt"
rm -f "$B"/*.o
python3 - "$B/image.map" <<'PY'
import re, sys
m = open(sys.argv[1]).read()
for n in ("FLASH", "SRAM"):
    r = re.search(r"^\s+%s\s+\w+\s+\w+\s+(\w+)" % n, m, re.M); print("%s used: %d B" % (n, int(r.group(1), 16)))
PY
echo "built $B/image.out ($BID)"
