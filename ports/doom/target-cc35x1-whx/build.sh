#!/bin/bash
# rp2040-doom engine (WHX data read in place) for the LP-EM-CC35X1. Engine sources, flags, shim and patch from ../m33-whx and
# ../../../third-party/rp2040-doom; platform pieces here; SDK example drivers/uart2echo as the base (FreeRTOS, the probe's serial port),
# TI Arm Clang 5.1.1.LTS, Toolbox image builder. Run ../m33-whx/convert.sh first (makes wad-cache/DOOM1.whx from the reader's own WAD).
#   ./build.sh [ZKB]        ZKB = zone size in kB (default 40; the 16-bit pointer window of 256 kB holds the zone and the engine's static data)
#   ZSTATS=1 ./build.sh     adds the zone high-water counter (default on; ZSTATS=0 to drop it)
#   HOTO2="" ./build.sh      the render files at -Os instead of the default -O2 (needs the pixels-init patch for equal checksums)
#   PANEL=1 ./build.sh      adds the Waveshare 3.5 inch panel driver (panel_cc35x1.c, wiring in PANEL-WIRING.md); default 0 = the image without a panel.
#                           PANEL_HZ=16000000 sets the SPI clock. Build only: nothing here flashes.
#   CRCALL=1 ./build.sh     frame checksum over every frame, not just the first 400 (slower; for the end-to-end check)
# Output: $RUN/ex/freertos/ticlang/doom.out and toolbox/tool_settings.json. Nothing here touches a board.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; DOOM="$(cd "$HERE/.." && pwd)"; REPO="$(cd "$DOOM/../.." && pwd)"
ZKB="${1:-32}"; RUN="${RUN:-$PWD/out/doom-cc35-whx/build}"
WHX="$DOOM/wad-cache/DOOM1.whx"; TP="$REPO/third-party/rp2040-doom"; M="$DOOM/m33-whx"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
[ -f "$WHX" ] || { echo "no $WHX: run ports/doom/fetch.sh, then m33-whx/convert.sh"; exit 2; }
[ -d "$TP/upstream/src" ] || "$TP/fetch.sh"
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/ex"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo" "$C"
D="$C/freertos/ticlang"
sed -i '' "s/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE ${STACKB:-4096}/" "$C/freertos/main_freertos.c"
cat >> "$C/freertos/uart2echo.syscfg" <<SYS

/* no over-the-air update: the application slot grows from 2,688 kB to 6,248 kB, room for the 1.8 MB game data */
var Mem_cfg = scripting.addModule("/ti/memoryconfig/MemoryConfigurator/Mem_cfg");
Mem_cfg.ota = false;
var FreeRTOS = scripting.addModule("/freertos/FreeRTOS");
FreeRTOS.heapSize = ${HEAPHEX:-0x2000};
SYS
if [ "${PANEL:-0}" = 1 ]; then
cat >> "$C/freertos/uart2echo.syscfg" <<SYS

/* Waveshare 3.5 inch panel (PANEL-WIRING.md): SPI0 on the BoosterPack header (SCLK BP.7 GPIO27, PICO BP.15 GPIO29, POCI BP.14 GPIO28) and four GPIO outputs.
   Three-pin SPI: chip select is a plain GPIO so the firmware can hold it high between commands. */
var SPI = scripting.addModule("/ti/drivers/SPI");
var spiP = SPI.addInstance();
spiP.\$name = "CONFIG_SPI_PANEL";
spiP.mode = "Three Pin";
spiP.\$hardware = system.deviceData.board.components.LP_SPI0_BUS;
var gp = [["CONFIG_GPIO_PANEL_CS", "boosterpack.18", "High"], ["CONFIG_GPIO_PANEL_DC", "boosterpack.37", "Low"],
          ["CONFIG_GPIO_PANEL_RST", "boosterpack.38", "High"], ["CONFIG_GPIO_PANEL_BL", "boosterpack.30", "Low"]];
for (var i = 0; i < gp.length; i++) {
    var g = GPIO.addInstance();
    g.\$name = gp[i][0];
    g.mode = "Output";
    g.initialOutputState = gp[i][2];
    g.gpioPin.\$assign = gp[i][1];
    scripting.suppress("Connected to hardware.*", g, "gpioPin");
}
SYS
fi
MK=(SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER=)
make -C "$D" "${MK[@]}" > "$RUN/b0.log" 2>&1 || true
[ -f "$D/ti_drivers_config.h" ] || { echo "SysConfig failed, see $RUN/b0.log"; tail -5 "$RUN/b0.log"; exit 1; }
# linker file: SDK's, with a 4 kB libc heap, the zone first in DRAM, then all of .bss in DRAM (engine objects first on the link line), so the
# zone and the engine's static data sit in one 256 kB window from 0x28000000 (the 16-bit pointers reach 256 kB)
python3 - "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo/freertos/ticlang/cc35xx_freertos.cmd" "$D/doom_link.cmd" <<'PY'
import sys
t = open(sys.argv[1]).read()
assert '--heap_size=0' in t and '    .sysmem         :    > DRAM' in t
g = t[t.index('    GROUP {\n        .bss:'):t.index('>> TCM_DRAM_NON_SECURE | DRAM | PSRAM') + len('>> TCM_DRAM_NON_SECURE | DRAM | PSRAM')]
t = t.replace('--heap_size=0', '--heap_size=4096')
t = t.replace(g, '    .bss  : {} align(4) > DRAM\n    .data : {} align(4) >> TCM_DRAM_NON_SECURE | DRAM | PSRAM')
t = t.replace('    .sysmem         :    > DRAM', '    /* Fox Red Labs: the engine zone first in DRAM */\n    .fox_zone : { *(.bss.fox_zone) } > DRAM\n\n    .sysmem         :    > DRAM', 1)
import os
# the game data file gets its own output section so .rodata can move to RAM without it
old = '    .rodata         :   > FLASH PALIGN(4)'
assert old in t
if os.environ.get('RODATA_RAM', '0') == '1':
    # RODATA_RAM=1: the engine's read-only tables are read with plain loads (44 cycles a word from the external flash, no data cache there):
    # keep them in flash, copy them to the fast RAM before main (BINIT)
    t = t.replace(old, '    .fox_whx : { *(.fox_whx) } > FLASH PALIGN(4096)\n    .rodata : {} load = FLASH PALIGN(4), run = TCM_DRAM_NON_SECURE, table(BINIT)')
else:
    t = t.replace(old, '    .fox_whx : { *(.fox_whx) } > FLASH PALIGN(4096)\n    .rodata         :   > FLASH PALIGN(4)')
tcm = os.environ.get('TCMSTAT', '1')
bss_old = '    .bss  : {} align(4) > DRAM'
if tcm == '1':
    # TCMSTAT=1: engine statics, zone and the RTOS heap (the main thread's stack) in the 128 kB TCM: a load or store there takes 1 cycle, in
    # DRAM 6 (measured, hal_cc35x1.c LAT lines). Frame buffers and the render list stay in DRAM. The 16-bit pointer window starts at 0x20000000.
    t = t.replace('    .fox_zone : { *(.bss.fox_zone) } > DRAM', '    .fox_zone : { *(.bss.fox_zone) } > TCM_DRAM_NON_SECURE', 1)
    lb = '*(.bss._ZL11list_buffer)' if os.environ.get('LISTBUF', 'tcm') == 'dram' else ''
    t = t.replace(bss_old, '    .fox_fb : { *(.bss.fox_fb) ' + lb + ' } > DRAM\n    .bss  : {} align(4) > TCM_DRAM_NON_SECURE', 1)
elif tcm == '2':
    # TCMSTAT=2 (experiment): only the two frame buffers in the TCM
    t = t.replace(bss_old, '    .fox_fb : { *(.bss.fox_fb) } > TCM_DRAM_NON_SECURE\n    .bss  : {} align(4) > DRAM', 1)
ramtext = os.environ.get('RAMTEXT', '')
if ramtext:
    # RAMTEXT="pd_render.o r_segs.o ...": the .text of those objects runs from the 32 kB code RAM (1.6 cycles to fetch, no cache misses)
    pats = ' '.join('*%s(.text*)' % o for o in ramtext.split())
    t = t.replace('    .TI.ramfunc     : {} load=FLASH, run=CRAM, table(BINIT)', '    .TI.ramfunc     : {} load=FLASH, run=CRAM, table(BINIT)\n    .fox_ramtext : { ' + pats + ' } load=FLASH, run=CRAM, table(BINIT), align(4)', 1)
if os.environ.get('LUMPCACHE', '1') == '1':
    t = t.replace('    .bss  : {}', '    .fox_arena_t (NOLOAD) : { *(.bss.fox_arena_t) } > TCM_DRAM_NON_SECURE\n    .bss  : {}', 1)
    t = t.replace('    .ramVecs ', '    .fox_arena_c (NOLOAD) : { *(.bss.fox_arena_c) } > CRAM\n    .ramVecs ', 1)
    t = t.replace('    .stack          :    > DRAM HIGH', '    .stack          :    > DRAM HIGH\n    .fox_arena_d (NOLOAD) : { *(.bss.fox_arena_d) } > DRAM HIGH', 1)
open(sys.argv[2], 'w').write(t)
PY
# engine: pinned upstream plus the m33-whx patch plus the demo-end hook
W="$RUN/src"; mkdir -p "$W"; cp -R "$TP/upstream/src" "$W/src"; cp -R "$TP/upstream/textscreen" "$W/textscreen"
for p in "$TP"/patches/*.patch "$HERE"/patches/*.patch; do patch -s -p1 -d "$W" < "$p"; done
U="$W/src"
[ -n "${PROF:-}" ] && python3 "$HERE/prof_patch.py" "$U/pd_render.cpp"
LCDEF=""
if [ "${LUMPCACHE:-1}" = 1 ]; then python3 "$HERE/lumpcache_patch.py" "$U/w_wad.h"; LCDEF="-DFOX_LC=1 -DLCT_KB=${LCT_KB:-2} -DLCD_KB=${LCD_KB:-180} -DLCL_KB=${LCL_KB:-60} -DLCC_KB=${LCC_KB:-28}"; fi
DEFS=""; for x in $(cat "$M/defs.txt"); do if [[ $x != SHORTPTR_BASE=* && $x != TINY_WAD_ADDR=* ]]; then DEFS="$DEFS -D$x"; fi; done
WINB=0x28000000; [ "${TCMSTAT:-1}" = 1 ] && WINB=0x20000000
DEFS="$DEFS -DSHORTPTR_BASE=$WINB -DTINY_WAD_ADDR=fox_whx"
[ "${ZSTATS:-1}" = 1 ] && DEFS="$DEFS -DFOX_ZSTATS=1"
DEFS="$DEFS $LCDEF"
[ "${PANEL:-0}" = 1 ] && DEFS="$DEFS -DFOX_PANEL=1"
[ -n "${CRCALL:-}" ] && DEFS="$DEFS -DCRC_FRAMES=1000000"
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CFLAGS="-Os $ARCH -w -include pico.h -include $HERE/fox_whx.h -ffunction-sections -fdata-sections $DEFS -I$M/shim -I$U -I$U/doom -I$U/pico -I$W/textscreen -I$U/opl -I$U/adpcm-xq"
mkdir -p "$RUN/eng"
PD=""
build() { f=$1; PD=""
  if [ -n "${PROF:-}" ]; then case $f in */doom/d_net.c) PD="-DG_Ticker=__wrap_G_Ticker";; */doom/d_main.c) PD="-DR_RenderPlayerView=__wrap_R_RenderPlayerView -Dpd_end_frame=__wrap_pd_end_frame -Dpd_begin_frame=__wrap_pd_begin_frame";; */pico/i_video.c) PD="-Dpd_core1_loop=__wrap_pd_core1_loop";; esac; fi
  for h in ${HOTO2-pd_render.cpp r_bsp.c r_data.c r_data_whd.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c}; do case $f in */$h) PD="$PD -O2";; esac; done
  case $f in *.cpp) L="-x c++ -std=c++17 -fno-exceptions -fno-rtti";; *) L="-std=gnu11";; esac
  "$CGT/bin/tiarmclang" $CFLAGS $PD $L -c "$f" -o "$RUN/eng/$(basename "${f%.*}").o"; }
for f in $(grep -v '^#' "$M/srclist.txt"); do build "$U/$f" & done; wait
PROFDEF=""; PROFLD=""
[ -n "${PROF:-}" ] && PROFDEF="-DFOX_PROF=1"
"$CGT/bin/tiarmclang" $CFLAGS $PROFDEF ${EXTRADEF:-} -DZONE_BYTES=$((ZKB*1024)) -c "$HERE/platform_core.c" -o "$RUN/eng/platform_core.o"
"$CGT/bin/tiarmclang" $ARCH -Os -w -I"$SDK/source" -I"$SDK/kernel/freertos" -I"$SDK/source/third_party/freertos/include" -I"$SDK/source/third_party/freertos/portable/GCC/ARM_CM33_NTZ/non_secure" -I"$D" -DDeviceFamily_CC35XX ${PROF:+-DFOX_PROF=1} ${EXTRADEF:-} -c "$HERE/hal_cc35x1.c" -o "$RUN/hal.obj"
HALX=""
if [ "${PANEL:-0}" = 1 ]; then
  "$CGT/bin/tiarmclang" $ARCH -Os -w -I"$SDK/source" -I"$SDK/kernel/freertos" -I"$SDK/source/third_party/freertos/include" -I"$SDK/source/third_party/freertos/portable/GCC/ARM_CM33_NTZ/non_secure" -I"$D" -DDeviceFamily_CC35XX ${PANEL_HZ:+-DPANEL_HZ=$PANEL_HZ} -c "$HERE/panel_cc35x1.c" -o "$RUN/panel.obj"
  HALX="$RUN/panel.obj"
fi
"$CGT/bin/tiarmclang" $ARCH -DWHX_PATH="\"$WHX\"" -c "$HERE/whx_blob.S" -o "$RUN/eng/whx_blob.o"
objs=$(ls "$D"/*.obj | grep -v '/uart2echo.obj')
( cd "$D" && "$CGT/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 "$RUN"/eng/*.o $objs "$RUN/hal.obj" $HALX "-L$SDK/source" ti_utils_build_linker.cmd.genlibs doom_link.cmd \
   -Wl,-m,doom.map -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc++.a -llibc++abi.a -llibc.a -o doom.out ) > "$RUN/link.log" 2>&1 || { echo "LINK FAILED"; tail -12 "$RUN/link.log"; exit 1; }
echo "zone ${ZKB} kB, linked: $D/doom.out"
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$D" BUILD_DIR="$D" BUILD_ARTIFACT="$D/doom.out" TOOLBOX_DIR="$TB" > "$RUN/sign.log" 2>&1 \
 || { echo "SIGN FAILED"; tail -6 "$RUN/sign.log"; exit 1; }
echo "ok: $D/toolbox/tool_settings.json"
python3 -I "$DOOM/target-cc35x1/sizes.py" "$D/doom.map" || true
# the 16-bit pointers reach 256 kB from 0x28000000: the statics the engine points at must be inside
"$CGT/bin/tiarmnm" "$D/doom.out" | python3 -c '
import sys
z = int(sys.argv[1]); bad = 0

for l in sys.stdin:
    p = l.split()
    if len(p) == 3 and p[2] in ("thinkercap", "players", "fox_zone"):
        a = int(p[0], 16); end = a + (z if p[2] == "fox_zone" else 0)
        print("%s at 0x%x%s" % (p[2], a, (", ends 0x%x" % end) if p[2] == "fox_zone" else ""))
        if end >= int(sys.argv[2], 16) + 0x40000: bad = 1
if bad: print("OUTSIDE the 256 kB window: shrink the zone"); sys.exit(1)
' $((ZKB*1024)) $WINB
