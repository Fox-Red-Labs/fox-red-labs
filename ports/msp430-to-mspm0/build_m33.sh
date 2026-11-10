#!/bin/bash
# Build one port for the LP-MSPM33C321A from the terminal: SysConfig, TI Arm Clang, the linker file SysConfig writes.
#   ./build_m33.sh <project> [-DFLAG ...]        e.g. ./build_m33.sh fram-persist     or  ./build_m33.sh fram-persist -DBURST=300
# Reads <project>/mspm33c321a/main.c and <project>/mspm33c321a/example.syscfg. Output: $OUT/<project><tag>/<project>.out (OUT defaults to
# out/msp430-ports/m33). The last 2 kB sector of the 1 MB main flash is cut from the linker's FLASH region so the code never lands where
# a project keeps its data. Checks that the map uses no NONMAIN (BCR/BSL) memory.
set -eu
P="$1"; shift
HERE="$(cd "$(dirname "$0")" && pwd)"
TAG=""; for a in "$@"; do TAG="$TAG$(echo "$a" | tr -c 'A-Za-z0-9=\n' '_')"; done
OUT="${OUT:-$PWD/out/msp430-ports/m33}/$P$TAG"
TOOLPATHS_FAMILY=mspm33-cc; . "$HERE/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang
SC=/Applications/ti/sysconfig_1.28.0
rm -rf "$OUT"; mkdir -p "$OUT/gen"
"$SC/nodejs/node" "$SC/dist/cli.js" --compiler ticlang --product "$SDK/.metadata/product.json" --quiet --output "$OUT/gen" "$HERE/$P/mspm33c321a/${SYSCFG:-example.syscfg}" > "$OUT/sysconfig.log" 2>&1 || { tail -15 "$OUT/sysconfig.log"; exit 1; }
sed -E 's/(FLASH +\(RX\) *: origin = 0x10000000, length = )0x000FFFFF/\10x000FF7FF/' "$OUT/gen/device_linker.cmd" > "$OUT/device_linker.cmd"
grep -q "0x000FF7FF" "$OUT/device_linker.cmd" || { echo "could not cut the data sector from the linker file"; exit 1; }
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CFLAGS="@$OUT/gen/device.opt -I$HERE/common -I$HERE/$P/mspm33c321a -I$OUT/gen -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -O2 $ARCH -gdwarf-3 -Wall $*"
OBJS=""
for f in "$HERE/$P/mspm33c321a/main.c" "$OUT"/gen/*.c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"; do
  o="$OUT/$(basename "${f%.c}").o"; "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$o" >> "$OUT/build.log" 2>&1 || { grep -m10 "error" "$OUT/build.log"; exit 1; }; OBJS="$OBJS $o"
done
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 $OBJS -L"$OUT/gen" -ldevice.cmd.genlibs "-L$SDK/source" "$OUT/device_linker.cmd" -Wl,-m,"$OUT/$P.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$OUT/$P.out" >> "$OUT/build.log" 2>&1 || { grep -m10 "error\|undefined" "$OUT/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$OUT/$P.map" | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}'
echo "built $OUT/$P.out: $(grep -E '^ +(FLASH|SRAM) ' "$OUT/$P.map" | awk '{print $1, "used", $4, "of", $3}' | tr '\n' ';')"
