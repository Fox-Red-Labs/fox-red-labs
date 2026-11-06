#!/bin/bash
# rp2040-doom engine for the LP-MSPM33C321A (target m33-whx), TI Arm Clang 5.1.1.LTS, emulator-friendly image.
#   m33-whx/build.sh [zone kB, default 44]            output $OUT/m33whx-z<zone>/doom.out and doom.map
#   Env: ZSTATS=1 adds the zone high-water counter; RAMLEN=0x003FFFFF links against 4 MB of RAM (measuring only: the
#        engine's 16-bit pointers need everything inside one 256 kB window); STACK, HEAP (bytes); G = SysConfig gen160 folder.
# No SysConfig start-up code, no clock set-up: this image runs in the emulator (host/emu_m33.py). Nothing here flashes a board.
#   BOARD=1 (needs WHXCACHE): the image for the LP-MSPM33C321A itself (E27 step 6). SysConfig output from board/doom_board.syscfg (160 MHz,
#        QSPI pins), plat/board.c, TI's QSPI_FLASH.c and driverlib; the page cache reads the QSPI through the SDK. WHX_FLASH_BASE (default
#        0x640000) is the flash offset of the file, WHX_CRC32 its CRC-32 (the firmware checks the file in flash against it before the demos).
#        MS_PER_READ (default 35) is the clock step per read, as emu_qspi.py --ms-per-read: 35 gives 3 game tics a frame.
#        This script only builds; flashing goes through tools/frl/lock.py and DSLite (E27-qspi-dump-and-load.md).
#   OPT=-O2 (default -Os) sets the engine's optimisation level; NOPXINIT=1 leaves out patch 0003.
#   EDU=1 (with BOARD=1 and WHXCACHE): the image with the Educational BoosterPack: Doom on the 128 x 96 window of its screen, stick and S1/S2 as keys
#        (plat/edu.c, board/doom_board_edu.syscfg). RT=1 runs the game on real time (default is the stepped clock of the plain board image, so checksums
#        compare); NOCRC=1 leaves the per-frame checksum out; SCRIPT=1 adds a scripted player (start at 8 s) for checks without hands;
#        HALTPAGE=1 stops on the first page (the title) for a look; TESTMS=<ms> holds a colour-bar test pattern at start.
#   Without BOARD, G (a SysConfig output folder) must exist; board/doom_board.syscfg's output works for the emulator image too.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; DOOM="$(cd "$HERE/.." && pwd)"; REPO="$(cd "$DOOM/../.." && pwd)"
ZKB=${1:-44}
TP="$REPO/third-party/rp2040-doom"
[ -d "$TP/upstream/src" ] || "$TP/fetch.sh"
TOOLPATHS_FAMILY=mspm33-cc . "$REPO/tools/frl/toolpaths.sh"
R="${R:-$PWD/out/mspm33c321a}"; G="${G:-$R/gen160}"
if [ -n "$BOARD" ]; then   # SysConfig output for the board image
  G="${BOARD_GEN:-$PWD/out/boosterpack-speedrun/m33-whx-board/gen}"; mkdir -p "$G"
  SYSCFG="$HERE/board/doom_board.syscfg"
  if [ -n "$EDU" ]; then SYSCFG="$HERE/board/doom_board_edu.syscfg"; G="${BOARD_GEN_EDU:-$PWD/out/doom-edumkii/gen-edu}"; mkdir -p "$G"; fi
  /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$SYSCFG" > "$G/sysconfig.log" 2>&1 || { tail "$G/sysconfig.log"; exit 2; }
  TAG="${TAG:-}-board"; [ -n "$EDU" ] && TAG="${TAG:-}-edu"
fi
[ -f "$G/device_linker.cmd" ] || { echo "No SysConfig output at $G: run mspm33c321a/prebuild.sh first, or set G to a folder with device_linker.cmd, device.opt and device.cmd.genlibs"; exit 2; }
EDEFS=""; [ -n "$EDU" ] && EDEFS="-DFOX_EDU=1"; [ -n "$RT" ] && EDEFS="$EDEFS -DFOX_RT=1"; [ -n "$NOCRC" ] && EDEFS="$EDEFS -DFOX_NO_FRAMECRC=1"; [ -n "$SCRIPT" ] && EDEFS="$EDEFS -DEDU_SCRIPT=1"; [ -n "$TESTMS" ] && EDEFS="$EDEFS -DEDU_TEST_MS=$TESTMS"; [ -n "$HALTPAGE" ] && EDEFS="$EDEFS -DEDU_HALT_ON_PAGE=1"
[ -n "$EDU" ] && { [ -n "$RT" ] && TAG="${TAG:-}-rt"; [ -n "$NOCRC" ] && TAG="${TAG:-}-nocrc"; [ -n "$SCRIPT" ] && TAG="${TAG:-}-script"; [ -n "$TESTMS" ] && TAG="${TAG:-}-test"; [ -n "$HALTPAGE" ] && TAG="${TAG:-}-page"; }
[ -n "$WHXCACHE" ] && TAG="${TAG:-}-c$WHXCACHE"; [ -n "$WHXSTATS" ] && TAG="${TAG:-}s"; [ -n "$WHXPAGE" ] && TAG="${TAG:-}-p$WHXPAGE"; [ -n "$NOPXINIT" ] && TAG="${TAG:-}-npx"; [ -n "$OPT" ] && TAG="${TAG:-}${OPT}"
OUT="${OUT:-$PWD/out/doom/target-builds}"; O="$OUT/m33whx-z$ZKB${TAG:-}"
W="${W_DIR:-$OUT/m33whx-src}"; mkdir -p "$O"
if [ -z "$W_REUSE" ]; then   # W_REUSE=1 compiles a hand-edited tree in W_DIR (used while writing patches/0002)
rm -rf "$W"; mkdir -p "$W"
cp -R "$TP/upstream/src" "$W/src"; cp -R "$TP/upstream/textscreen" "$W/textscreen"
for p in "$TP"/patches/*.patch; do patch -s -p1 -d "$W" < "$p"; done
[ -n "$WHXCACHE" ] && patch -s -p1 -d "$W" < "$HERE/patches/0002-whx-page-cache.patch"
# 0003 (always applied since 2026-10-21): zero the stack array the composite-column drawer partly leaves unwritten, so the frames do not
# depend on what the stack held (the compiler's -O level and the code around it). NOPXINIT=1 leaves it out (the control of E235 section 12).
[ -z "$NOPXINIT" ] && patch -s -p1 -d "$W" < "$HERE/patches/0003-pixels-init.patch"
fi
U="$W/src"
DEFS=$(for x in $(cat "$HERE/defs.txt"); do printf -- "-D%s " "$x"; done)
[ -n "$ZSTATS" ] && DEFS="$DEFS -DFOX_ZSTATS=1"
[ -n "$BOARD" ] && DEFS="$DEFS -DFOX_BOARD=1"
DEFS="$DEFS $EDEFS"
# WHXCACHE=<kB>: the engine reads the WHX through a software page cache of that many kB (256-byte pages, 2 ways) filled over the QSPI;
# WHXSTATS=1 adds the hit and miss counters (a few instructions per access, so leave it off when counting instructions).
[ -n "$WHXCACHE" ] && DEFS="$DEFS -DFOX_WHX_CACHE=1 -DFOX_WHX_KB=$WHXCACHE"
[ -n "$WHXSTATS" ] && DEFS="$DEFS -DFOX_WHX_STATS=1"
[ -n "$WHXPAGE" ] && DEFS="$DEFS -DFOX_WHX_PAGE_SHIFT=$WHXPAGE"   # page size as a power of two: 8 = 256 bytes (default), 7 = 128, 6 = 64
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CFLAGS="${OPT:--Os} $ARCH -w -include pico.h -ffunction-sections -fdata-sections $DEFS -I$HERE/shim -I$U -I$U/doom -I$U/pico -I$W/textscreen -I$U/opl -I$U/adpcm-xq"
mkdir -p "$O/obj"
if [ -n "$BOARD" ] && [ -z "$W_REUSE" ]; then   # the emulator hooks the entry of G_CheckDemoStatus; the image calls fox_board_demo_end() there
  perl -0pi -e 's/boolean G_CheckDemoStatus \(void\) \n\{/void fox_board_demo_end(void);\nboolean G_CheckDemoStatus (void) \n{ fox_board_demo_end();/' "$U/doom/g_game.c"
  grep -q fox_board_demo_end "$U/doom/g_game.c" || { echo "G_CheckDemoStatus hook not applied"; exit 2; }
fi
build() { # file
  f=$1; case $f in *.cpp) L="-x c++ -std=c++17 -fno-exceptions -fno-rtti";; *) L="-std=gnu11";; esac
  "$CC/bin/tiarmclang" $CFLAGS $L $EXTRA -c "$f" -o "$O/obj/$(basename "${f%.*}").o"
}
for f in $(grep -v '^#' "$HERE/srclist.txt"); do build "$U/$f" & done; wait
"$CC/bin/tiarmclang" $CFLAGS -DZONE_BYTES=$((ZKB*1024)) -c "$HERE/plat/platform.c" -o "$O/obj/platform.o"
[ -n "$WHXCACHE" ] && "$CC/bin/tiarmclang" $CFLAGS -std=gnu11 -c "$HERE/plat/fox_whx.c" -o "$O/obj/fox_whx.o"
if [ -n "$BOARD" ]; then
  BF="-Os $ARCH -w -std=gnu11 -DFOX_BOARD=1 $EDEFS -DWHX_FLASH_BASE=${WHX_FLASH_BASE:-0x640000}u -DWHX_CRC32=${WHX_CRC32:-0}u -DWHX_BYTES=${WHX_BYTES:-1800312}u -DFOX_MS_PER_READ=${MS_PER_READ:-35} @$G/device.opt -I$G -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include"
  "$CC/bin/tiarmclang" $BF -c "$HERE/plat/board.c" -o "$O/obj/board.o"
  "$CC/bin/tiarmclang" $BF -c "$G/ti_msp_dl_config.c" -o "$O/obj/ti_msp_dl_config.o"
  if [ -n "$EDU" ]; then
    EP="$REPO/portability/boosterpacks/edumkii"
    for f in "$HERE/plat/edu.c" "$EP/src/edumkii_io.c" "$EP/src/edumkii_lcd.c" "$EP/src/edumkii_font5x7.c"; do
      "$CC/bin/tiarmclang" $BF -I"$EP/include" -c "$f" -o "$O/obj/$(basename "${f%.*}").o" &
    done; wait
  fi
  "$CC/bin/tiarmclang" $BF -c "$SDK/source/ti/external_flash/QSPI_FLASH.c" -o "$O/obj/QSPI_FLASH.o"
fi
SED_RAM="s/0x0003FFFF/${RAMLEN:-0x0003FFFF}/"
sed -e "$SED_RAM" -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-8192}/" -e "s/^MEMORY/--heap_size=${HEAP:-4096}\n\nMEMORY/" "$G/device_linker.cmd" > "$O/device_linker.cmd"
"$CC/bin/tiarmclang" -Os $ARCH @"$G/device.opt" -I"$SDK/source" -I"$SDK/source/third_party/CMSIS/Core/Include" -w -c "$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c" -o "$O/startup.o"
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 "$O"/obj/*.o "$O/startup.o" -l"$G/device.cmd.genlibs" "-L$SDK/source" "$O/device_linker.cmd" \
  -Wl,-m,"$O/doom.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc++.a -llibc++abi.a -llibc.a -o "$O/doom.out"
echo "built $O/doom.out"
python3 -I "$DOOM/target/sizes.py" "$O/doom.out" "$CC/bin/tiarmobjdump" | tail -2
