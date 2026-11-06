#!/bin/bash
# Build the post-quantum join core for the LP-MSPM0G3507 (Cortex-M0+, 80 MHz, 32 kB RAM) with TI Arm Clang. BUILD ONLY: no probe,
# no serial port, no board (the G3507 is on loan). Prints flash and RAM from the linker map.
#   ./build.sh [-O2|-Os|-O3] [stack bytes]      defaults -Os and 12288
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
OPT="${1:--Os}"; STACK="${2:-12288}"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
GEN="$REPO/mspm0g3507/third-party-bench/syscfg"; PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"
OUT="$HERE/out/${OPT}-${STACK}"; rm -rf "$OUT"; mkdir -p "$OUT"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
CL="$CC/bin/tiarmclang"
COM=($ARCH @"$GEN/device.opt" -I"$GEN" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source" -I"$PJ" -I"$ML" -DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -gdwarf-3 -Wall)
ok=1
cc1() { "$CL" "$@" >> "$OUT/build.log" 2>&1 || ok=0; }
cc1 -O2 "${COM[@]}" -c "$HERE/board_g3507.c" -o "$OUT/board.o"
cc1 -O2 "${COM[@]}" -c "$GEN/ti_msp_dl_config.c" -o "$OUT/cfg.o"
cc1 -O2 "${COM[@]}" -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c" -o "$OUT/startup.o"
cc1 "$OPT" "${COM[@]}" -c "$ML/mlkem_native.c" -o "$OUT/mlkem.o"
cc1 "$OPT" "${COM[@]}" -c "$PJ/pqc_join.c" -o "$OUT/join.o"
cc1 "$OPT" "${COM[@]}" -c "$PJ/pqc_sym.c" -o "$OUT/sym.o"
[ $ok = 1 ] || { echo "COMPILE FAILED"; grep -E "error" "$OUT/build.log" | head; exit 1; }
sed "s/--stack_size=512/--stack_size=$STACK/" "$GEN/device_linker.cmd" > "$OUT/linker.cmd"
( cd "$OUT" && "$CL" $ARCH -Wl,-u,_c_int00 ./*.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./linker.cmd -Wl,-m,image.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o image.out ) > "$OUT/link.log" 2>&1
rc=$?
if [ $rc != 0 ]; then echo "LINK FAILED (stack $STACK):"; grep -iE "error|overflow|cannot|placement" "$OUT/link.log" | head -6; rm -f "$OUT"/*.o; exit 2; fi
"$CC/bin/tiarmsize" "$OUT/image.out" | tee "$OUT/size.txt"
python3 - "$OUT/image.map" <<'PY'
import re, sys
m = open(sys.argv[1]).read()
for n in ("FLASH", "SRAM"):
    r = re.search(r"^\s+%s\s+\w+\s+\w+\s+(\w+)" % n, m, re.M); print("%s used: %d B" % (n, int(r.group(1), 16)))
r = re.search(r"^\s+mlkem\.o\s+(\d+)\s+(\d+)", m, re.M)
if r: print("mlkem-native object: code %s B, read-only data %s B" % r.groups())
PY
rm -f "$OUT"/*.o
