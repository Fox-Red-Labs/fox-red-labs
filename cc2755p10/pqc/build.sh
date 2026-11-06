#!/bin/bash
# Build the post-quantum timing image for the LP-EM-CC2755P10 from the terminal. BUILD ONLY: never touches a probe or a board.
#   ./build.sh [-O3|-Os|-O2]      (default -O3) -> $PQC_OUT/out/pqc-<opt>/image.out, image.map, image.hex, build.log, ccfg.txt
# Same method as cc1354p10/pqc-join/build.sh (mlkem-native and mldsa-native unmodified, portability/pqc-join/core) on the
# F3 SDK 9.21.00.36 with TI Arm Clang 5.1.1.LTS and SysConfig 1.26.3, the tools of cc2755p10/bench/build.sh.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
HERE_OUT="${PQC_OUT:-$HERE}"; mkdir -p "$HERE_OUT"
OPT="${1:--O3}"; NAME="pqc${OPT}"
BOARD=cc2755p10; FILTER=""
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
SYSCFG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
TFM_INC="-I$SDK/tfm_s/build/cc27xxx10/production_full/Release/export/tfm/inc -I$SDK/source/third_party/tfm/interface/include"
ARCH="-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 $TFM_INC -Wl,--search_path=$SDK/tfm_s"
BB="$REPO/cc2755p10/bench/board"; LIB="$REPO/cc2340r5/bench/lib"
CCFG_KIND=cc23
SRC_DIR="$HERE"; HERE="$HERE_OUT"   # build-lib.sh puts out/ under $HERE
. "$LIB/build-lib.sh" 2>/dev/null
OUT="$HERE_OUT/out"; B="$OUT/$NAME"; rm -rf "$B"; mkdir -p "$B"
G=$(gen "$SRC_DIR/pqc.syscfg") || exit 1
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
BID="$BOARD-$NAME-$(date +%Y%m%d%H%M%S)"
COMMON=(-mthumb $ARCH "@$G/ti_utils_build_compiler.opt" -I"$G" -I"$LIB" "-I$SDK/source" "-I$SDK/kernel/nortos" "-I$SDK/kernel/nortos/posix" -gdwarf-3 -Wall)
MLF=(-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I"$ML" -I"$PJ")
MDF=(-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API -I"$MD" -I"$PJ")
: > "$B/build.log"; ok=1
cc1() { echo "$CLANG $*" >> "$B/build.log"; "$CLANG" "$@" >> "$B/build.log" 2>&1 || ok=0; }
for f in "$BB/main_nortos.c" "$G"/ti_*.c; do [ -f "$f" ] && cc1 -O2 "${COMMON[@]}" -c "$f" -o "$B/$(basename "${f%.c}").o"; done
cc1 -O2 "${COMMON[@]}" "${MLF[@]}" -I"$MD" -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"$OPT\"" -c "$SRC_DIR/pqc_board.c" -o "$B/pqc_board.o"
cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -c "$ML/mlkem_native.c" -o "$B/mlkem_native.o"
cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -c "$PJ/pqc_join.c" -o "$B/pqc_join.o"
cc1 "$OPT" "${COMMON[@]}" -c "$PJ/pqc_sym.c" -o "$B/pqc_sym.o"
cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$MD/mldsa_native.c" -o "$B/mldsa_native.o"
cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$PJ/pqc_sign.c" -o "$B/pqc_sign.o"
if [ $ok = 1 ]; then
	sed -E "s/^--stack_size=[0-9]+/--stack_size=${STACK:-65536}/; s/^--heap_size=[0-9]+/--heap_size=0/" "$BB/lpf3_nortos.cmd" > "$B/linker.cmd"
	( cd "$B" && "$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o "-L$SDK/source" "-L$SDK/kernel/nortos" "-Wl,--search_path=$G" \
		"$G/ti_utils_build_linker.cmd.genlibs" ./linker.cmd "-Wl,-m,image.map" -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o image.out ) >> "$B/build.log" 2>&1 || ok=0
fi
[ $ok = 1 ] && { "$SDK/tools/common/crc_tool/crc_tool" patch-image --elf "$B/image.out" --symbol-prefix ti_utils_build_GenMap_sym_CRC --output "$B/image.out" >> "$B/build.log" 2>&1 || ok=0; }
[ $ok = 1 ] && { "$CGT/bin/tiarmobjcopy" -O ihex "$B/image.out" "$B/image.hex" >> "$B/build.log" 2>&1 || ok=0; }
if [ $ok = 0 ]; then echo "BUILD FAILED"; grep -iE "error|undefined" "$B/build.log" | head -15; exit 1; fi
cc=$(ccfg_check "$G") || { echo "CCFG CHECK FAILED: $cc"; exit 1; }
echo "$cc" > "$B/ccfg.txt"; echo "$BID" > "$B/build-id.txt"
mkdir -p "$B/objs"; mv "$B"/*.o "$B/objs/" 2>/dev/null
python3 - "$B/image.map" <<'PY'
import re, sys
m = open(sys.argv[1]).read()
for n in ("FLASH", "SRAM"):
    r = re.search(r"^\s+%s\s+\w+\s+\w+\s+(\w+)" % n, m, re.M); print("%s used: %d B" % (n, int(r.group(1), 16)))
PY
echo "built $B/image.out ($BID)"
