#!/bin/bash
# Build one Cmsemicon CMS32F04xx example for the LP-MSPM0L1306 with TI Arm Clang 5.1.1.
#   ./build.sh <example path under upstream/examples>   e.g. ./build.sh systick  or  ./build.sh timer/timer1
# The example's main.c is compiled as shipped. If ported/<name>/main.c exists it is used instead
# (an "edited" build: the diff against upstream is the edit count). The Cmsemicon *_demo.c glue
# files are replaced by glue/l1306/*.c. Output: build/<name>/<name>.out and a .map.
set -e
EX="$1"; [ -n "$EX" ] || { echo "usage: build.sh <example>"; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../.."
NAME="${EX//\//_}"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
GLUE="$HERE/glue/l1306"
UP="$HERE/upstream/examples/$EX"
MAIN="$UP/main.c"; KIND=unchanged
[ -f "$HERE/ported/$NAME/main.c" ] && { MAIN="$HERE/ported/$NAME/main.c"; KIND=edited; }
[ -f "$MAIN" ] || { echo "no main.c for $EX"; exit 2; }
OUT="$HERE/build/$NAME"; rm -rf "$OUT"; mkdir -p "$OUT"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
CFLAGS="-O2 -fno-builtin $ARCH -D__MSPM0L1306__ -DCMS_EXAMPLE_$NAME -I$GLUE -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -ffunction-sections -fdata-sections"
# the upstream files are GBK-encoded; the compiler only needs the code, so transcode a copy
iconv -f GBK -t UTF-8 -c "$MAIN" > "$OUT/main.c" 2>/dev/null || cp "$MAIN" "$OUT/main.c"
OBJS=""
# main.c at -O0: Cmsemicon's own Keil projects build at optimization level 0, and the examples poll non-volatile globals
"$CC/bin/tiarmclang" ${CFLAGS/-O2/-O0} -c "$OUT/main.c" -o "$OUT/main.o"; OBJS="$OUT/main.o"
for f in $(ls "$GLUE"/*.c | grep -v startup_l1306); do b=$(basename "$f" .c); "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$OUT/$b.o"; OBJS="$OBJS $OUT/$b.o"; done
"$CC/bin/tiarmclang" $CFLAGS -c "$GLUE/startup_l1306.c" -o "$OUT/startup.o"; OBJS="$OBJS $OUT/startup.o"
sed -E "s/--stack_size=[0-9]+/--stack_size=${STACK:-768}/" "$HERE/glue/l1306/device_linker.cmd" > "$OUT/device_linker.cmd"
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 $OBJS -l"$HERE/glue/l1306/device.cmd.genlibs" "-L$SDK/source" "$OUT/device_linker.cmd" \
  -Wl,-m,"$OUT/$NAME.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$OUT/$NAME.out"
echo "$KIND build: $OUT/$NAME.out  $(grep -E '^ +(FLASH|SRAM) ' "$OUT/$NAME.map" | while read n o l u r; do printf "%s used %d of %d  " "$n" "0x$u" "0x$l"; done)"
