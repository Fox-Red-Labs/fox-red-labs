#!/bin/bash
# Build one port for the LP-MSPM0L1306 from the terminal: SysConfig, TI Arm Clang, the SDK's own linker file.
#   ./build_l1306.sh <project> [-DFLAG ...]        e.g. ./build_l1306.sh lpm3-wake     or  ./build_l1306.sh fram-persist -DBURST=200
# Reads <project>/l1306/main.c and <project>/l1306/example.syscfg. Output: $OUT/<project><tag>/<project>.out (OUT defaults to
# out/msp430-ports/l1306). The last 1 kB sector of the 64 kB flash is cut from the linker's FLASH
# region (DATA_SECTOR_RESERVE=1) so the code never lands where a project keeps its data.
set -eu
P="$1"; shift
HERE="$(cd "$(dirname "$0")" && pwd)"
TAG=""; for a in "$@"; do TAG="$TAG$(echo "$a" | tr -c 'A-Za-z0-9=\n' '_')"; done
OUT="${OUT:-$PWD/out/msp430-ports/l1306}/$P$TAG"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SC=/Applications/ti/sysconfig_1.28.0
rm -rf "$OUT"; mkdir -p "$OUT/gen"
"$SC/nodejs/node" "$SC/dist/cli.js" --compiler ticlang --product "$SDK/.metadata/product.json" --quiet --output "$OUT/gen" "$HERE/$P/l1306/${SYSCFG:-example.syscfg}" > "$OUT/sysconfig.log" 2>&1 || { tail -15 "$OUT/sysconfig.log"; exit 1; }
EX="$SDK/examples/nortos/LP_MSPM0L1306/driverlib/flashctl_multiple_size_write/ticlang/device_linker.cmd"
sed -E 's/length = 0x0000FFF8/length = 0x0000F800/' "$EX" > "$OUT/device_linker.cmd"
CFLAGS="@$OUT/gen/device.opt -I$HERE/common -I$HERE/$P/l1306 -I$OUT/gen -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -O2 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -gdwarf-3 -Wall $*"
OBJS=""
for f in "$HERE/$P/l1306/main.c" "$OUT"/gen/*.c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0l130x_ticlang.c"; do
  o="$OUT/$(basename "${f%.c}").o"; "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$o" >> "$OUT/build.log" 2>&1 || { grep -m10 "error" "$OUT/build.log"; exit 1; }; OBJS="$OBJS $o"
done
"$CC/bin/tiarmclang" -Wl,-u,_c_int00 $OBJS -L"$OUT/gen" -ldevice.cmd.genlibs "-L$SDK/source" "$OUT/device_linker.cmd" -Wl,-m,"$OUT/$P.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$OUT/$P.out" >> "$OUT/build.log" 2>&1 || { grep -m10 "error\|undefined" "$OUT/build.log"; exit 1; }
echo "built $OUT/$P.out: $(grep -E '^ +(FLASH|SRAM) ' "$OUT/$P.map" | awk '{print $1, "used", $4, "of", $3}' | tr '\n' ';')"
