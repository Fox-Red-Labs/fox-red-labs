#!/bin/bash
# Doom engine (doomgeneric, GPL-2.0) for the LP-EM-CC35X1: engine from ../src-upstream (run ../fetch.sh first), DOOM1.WAD from ../wad-cache,
# SDK example drivers/uart2echo as the base (FreeRTOS, the probe's serial port), TI Arm Clang 5.1.1.LTS, Toolbox image builder.
#   ./build.sh [ZKB]        ZKB = zone size in KiB; default "auto" = everything the linker has left in DRAM
#   STATS=1 ./build.sh      adds the engine's zone counters (slower: walks the block list on every allocation)
#   OPT=Os|O2|O3            engine optimisation (default Os)
# Output: $RUN/ex/freertos/ticlang/doom.out and toolbox/tool_settings.json (flash with flash_run.sh). Nothing here touches a board.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; DOOM="$(cd "$HERE/.." && pwd)"; REPO="$(cd "$DOOM/../.." && pwd)"
ZKB="${1:-auto}"; OPT="${OPT:-Os}"; RUN="${RUN:-$PWD/out/doom-cc35/build}"
SRC="${SRC:-$DOOM/src-upstream/doomgeneric}"; WAD="$DOOM/wad-cache/DOOM1.WAD"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
[ -f "$WAD" ] || { echo "no $WAD: run ports/doom/fetch.sh"; exit 2; }
[ "$(shasum "$WAD" | cut -d' ' -f1)" = 5b2e249b9c5133ec987b3ea77596381dc0d6bc1d ] || { echo "WAD SHA-1 differs"; exit 2; }
[ -d "$SRC" ] || { echo "no engine source: run ports/doom/fetch.sh"; exit 2; }
. "$DOOM/srclist.sh"
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/ex"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo" "$C"
D="$C/freertos/ticlang"
sed -i '' 's/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE 16384/' "$C/freertos/main_freertos.c"
cat >> "$C/freertos/uart2echo.syscfg" <<SYS

/* no over-the-air update: the application slot grows from 2,688 kB to 6,248 kB, room for the 4 MB WAD */
var Mem_cfg = scripting.addModule("/ti/memoryconfig/MemoryConfigurator/Mem_cfg");
Mem_cfg.ota = false;
/* small FreeRTOS heap: the 16 kB thread stack plus the driver's few objects; the rest of the RAM is the zone */
var FreeRTOS = scripting.addModule("/freertos/FreeRTOS");
FreeRTOS.heapSize = ${HEAPHEX:-0x5800};
SYS
MK=(SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER=)
# 1. SysConfig output and the SDK objects, built with the example's own uart2echo.c, then replaced by ours
make -C "$D" "${MK[@]}" > "$RUN/b0.log" 2>&1 || true
cp "$HERE/dg_cc35x1.c" "$C/uart2echo.c"
[ -f "$D/ti_drivers_config.h" ] || { echo "SysConfig failed, see $RUN/b0.log"; tail -5 "$RUN/b0.log"; exit 1; }
# 2. linker file: SDK's, with a small libc heap and the zone placed first in the big DRAM block
python3 - "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo/freertos/ticlang/cc35xx_freertos.cmd" "$D/doom_link.cmd" <<'PY'
import sys
t = open(sys.argv[1]).read()
assert '--heap_size=0' in t and '    GROUP {\n        .bss:' in t and '>> TCM_DRAM_NON_SECURE | DRAM | PSRAM' in t
t = t.replace('--heap_size=0', '--heap_size=4096')
t = t.replace('    GROUP {\n        .bss:', '    /* Fox Red Labs: the engine zone alone fills DRAM first; everything else goes to TCM, then DRAM */\n    .fox_zone : { *(.bss.fox_zone) *(.bss.*fox_zone*) } > DRAM\n\n    GROUP {\n        .bss:', 1)
t = t.replace('>> TCM_DRAM_NON_SECURE | DRAM | PSRAM', '>> TCM_DRAM_NON_SECURE | CRAM | DRAM | PSRAM', 1)   # the 32 kB code RAM also takes plain data
open(sys.argv[2], 'w').write(t)
PY
# 3. engine objects
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
SQ=$(. "$DOOM/srclist.sh"; echo "$SQ_S3")
zone_flag() { echo "-DFOX_ZONE_STATIC=$(( $1 * 1024 ))"; }
build_objs() {  # $1 = zone KiB
  local zf; zf=$(zone_flag "$1"); [ -n "${STATS:-}" ] && zf="$zf -DFOX_ZSTATS"
  CF="-$OPT $ARCH -I$SRC -I$HERE -I$DOOM/target -I$DOOM/target/include -I$SDK/source -I$SDK/kernel/freertos -I$SDK/source/third_party/freertos/include -I$SDK/source/third_party/freertos/portable/GCC/ARM_CM33_NTZ/non_secure -I$D -DDeviceFamily_CC35XX -gdwarf-3 -w -ffunction-sections -fdata-sections \
 ${DEMO:+-DFOX_DEMO=$DEMO} -DFOX_DIRECT -DFOX_WAD_MEM -DNORMALUNIX -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -include $DOOM/target/fox_target.h $SQ $zf"
  mkdir -p "$RUN/eng"; rm -f "$RUN"/eng/*.o
  for f in $SRCS; do [ "$f" = doomgeneric ] && continue; "$CGT/bin/tiarmclang" $CF -c "$SRC/$f.c" -o "$RUN/eng/$f.o" & done; wait
  "$CGT/bin/tiarmclang" $CF -c "$SRC/doomgeneric.c" -o "$RUN/eng/doomgeneric.o"
  "$CGT/bin/tiarmclang" $CF -c "$C/uart2echo.c" -o "$RUN/eng/dg_cc35x1.o"
  "$CGT/bin/tiarmclang" $ARCH -DWAD_PATH="\"$WAD\"" -c "$HERE/wad_blob.S" -o "$RUN/eng/wad_blob.o"
}
link() {
  local objs; objs=$(ls "$D"/*.obj | grep -v '/uart2echo.obj')
  ( cd "$D" && "$CGT/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 $objs "$RUN"/eng/*.o "-L$SDK/source" ti_utils_build_linker.cmd.genlibs doom_link.cmd -Wl,-m,doom.map -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o doom.out ) > "$RUN/link.log" 2>&1
}
Z0=64; [ "$ZKB" != auto ] && Z0=$ZKB
build_objs "$Z0"; link || { echo "LINK FAILED"; tail -8 "$RUN/link.log"; exit 1; }
if [ "$ZKB" = auto ]; then
  # DRAM left after a 64 kB zone: the free space of the DRAM block in the map
  FREE=$(python3 - "$D/doom.map" <<'PY'
import re, sys
for l in open(sys.argv[1]):
    p = l.split()
    if len(p) >= 6 and p[0] == 'DRAM':
        print(int(p[4], 16)); break
PY
)
  Z=$(( Z0 + FREE / 1024 - 1 ))
  echo "DRAM free after a ${Z0} kB zone: $FREE B, zone becomes ${Z} kB"
  build_objs "$Z"; link || { echo "LINK FAILED at ${Z} kB"; tail -8 "$RUN/link.log"; exit 1; }
  ZKB=$Z
fi
echo "zone ${ZKB} kB, linked: $D/doom.out"
# 4. signed images
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$D" BUILD_DIR="$D" BUILD_ARTIFACT="$D/doom.out" TOOLBOX_DIR="$TB" > "$RUN/sign.log" 2>&1 \
 || { echo "SIGN FAILED"; tail -6 "$RUN/sign.log"; exit 1; }
echo "ok: $D/toolbox/tool_settings.json"
python3 -I "$HERE/sizes.py" "$D/doom.map" || true
