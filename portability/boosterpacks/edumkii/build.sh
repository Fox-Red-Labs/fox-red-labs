#!/bin/bash
# Build the hello example for one board.
#   [APP_HOOK=<script> sources an app from another folder, see below] [AUTOSTART=1] [APP=framebench|snake|tileworld|crate-confusion|gameboy|llama2c] build.sh <board> [--lcd|--no-lcd] [out_dir]     (APP is an examples/ folder, default hello)
# Game Boy speed switches (APP=gameboy): EDU_CLK160=1 (CPU at 160 MHz), GB_FLAGS=-DPGB_FRAME_SKIP[@-DPGB_RAMFUNC] (@ is a space), GB_OPT=-O2, GB_UNPACED=1 (no 59.7 fps pacing), EDU_TAG=name (suffix of the output folder).
# Boards: mspm33c321a mspm0g3519 mspm0g3507 mspm0g5187 mspm0l1306
# --lcd on mspm0l1306 builds the screen path as a build check (it needs jumper wires, see README).
# Output: <out_dir>/<board>[-lcd]/ticlang/edumkii.out, edumkii.map, and a one-line size summary.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
BOARD="${1:?board}"; shift
LCDARG=""; OUT="$PWD/out/boosterpack-speedrun/edumkii"
for a in "$@"; do case "$a" in --lcd|--no-lcd) LCDARG="$a";; *) OUT="$a";; esac; done
TAG="$BOARD"; [ "$LCDARG" = "--lcd" ] && TAG="$BOARD-lcd"; [ -n "$APP" ] && [ "$APP" != hello ] && TAG="$TAG-$APP"
# EDU_CLK160=1 runs the M33 CPU at 160 MHz (PLL on the crystal) instead of the 32 MHz default; the tag gets -160. EDU_TAG adds a suffix.
export EDU_CLK160; [ "$EDU_CLK160" = 1 ] && TAG="$TAG-160"; [ -n "$EDU_TAG" ] && TAG="$TAG-$EDU_TAG"
O="$OUT/$TAG"
case "$BOARD" in
  mspm33c321a) FAM=mspm33-cc; EXDIR=LP_MSPM33C321A; EX=gpio_toggle_output;;
  mspm0g3519)  FAM=mspm0-cc;  EXDIR=LP_MSPM0G3519;  EX=empty;;
  mspm0g3507)  FAM=mspm0-cc;  EXDIR=LP_MSPM0G3507;  EX=empty;;
  mspm0g5187)  FAM=mspm0-cc;  EXDIR=LP_MSPM0G5187;  EX=empty;;
  mspm0l1306)  FAM=mspm0-cc;  EXDIR=LP_MSPM0L1306;  EX=empty;;
  *) echo "unknown board $BOARD"; exit 2;;
esac
TOOLPATHS_FAMILY=$FAM; . "$REPO/tools/frl/toolpaths.sh"        # SDK and CC
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; mkdir -p "$OUT"
cp -R "$SDK/examples/nortos/$EXDIR/driverlib/$EX" "$O"
rm -rf "$O/gcc" "$O/iar" "$O/keil" "$O/README".* "$O/ti_msp_dl_config".*
python3 "$HERE/tools/gen_board.py" "$BOARD" "$O/gen" $LCDARG
cp "$O/gen/edumkii.syscfg" "$O/$EX.syscfg"
APP="${APP:-hello}"
CORE="$HERE/src/edumkii_io.c $HERE/src/edumkii_font5x7.c $HERE/src/edumkii_lcd.c $HERE/boards/common/port_dl.c"
FLAGS="-I$HERE/include -I$HERE/boards/common -I$O/gen"
if ! grep -q EDU_HAS_LCD "$O/gen/board_config.h"; then FLAGS="$FLAGS -DEDUMKII_NO_LCD"; fi
EXTRA_CXX=""      # C++ sources, each "path|extra flags"
if [ "$APP" = snake ] || [ "$APP" = tileworld ] || [ "$APP" = crate-confusion ]; then
  # Arduboy games from third-party/arduboy-games through the Arduboy2 layer in ports/arduboy (read only: nothing there is edited)
  AB="$REPO/ports/arduboy"; GD="$REPO/third-party/arduboy-games/$APP"
  echo "int edu_unused_main_stub;" > "$O/$EX.c"
  PRE="-include $AB/tools/ino_prelude.h"
  RN=""; [ "$APP" = crate-confusion ] && RN="-DFRL_RENAME_MODE_T"
  EXTRA_CXX=""
  for f in "$GD"/*.ino; do
    python3 "$AB/tools/ino_protos.py" "$f" "$O/$(basename "$f").cpp"
    EXTRA_CXX="$EXTRA_CXX$O/$(basename "$f").cpp|$PRE $RN;"
  done
  for f in "$GD"/*.cpp; do [ -e "$f" ] && EXTRA_CXX="$EXTRA_CXX$f|$PRE $RN;"; done
  EXTRA_CXX="$EXTRA_CXX$AB/src/arduboy2.cpp|$RN;$AB/src/frl_core.cpp|$RN;$HERE/examples/snake/frl_edumkii.cpp|"
  CXXF="-I$AB/include -I$GD"
  [ -n "$AUTOSTART" ] && CXXF="$CXXF -DEDU_SNAKE_AUTOSTART"
elif [ "$APP" = gameboy ]; then
  # Peanut-GB from ports/peanut-gb (patched copy), ROM Tobu Tobu Girl linked into flash. The ROM stays in a git-ignored folder.
  AB="$REPO/ports/arduboy"; PG="$REPO/ports/peanut-gb"
  [ -f "$PG/roms/tobu.gb" ] || bash "$PG/fetch_roms.sh"
  bash "$PG/patches/apply.sh" "$PG/out/peanut-gb-patched" > /dev/null
  echo "int edu_unused_main_stub;" > "$O/$EX.c"
  python3 "$AB/tools/bin2c.py" "$PG/roms/tobu.gb" "$O/rom_data.c" frl_rom_data frl_rom_len
  # Speed switches (examples/gameboy, ports/peanut-gb/README.md "Speed on the M33"): front end at -O2, no sprite sorting, line scaler with a
  # table, rows streamed by DMA. GB_FLAGS adds more, with @ for spaces: GB_FLAGS=-DPGB_FRAME_SKIP (draw one frame in two),
  # -DPGB_RAMFUNC (emulator core in SRAM), -DPGB_SCALER_AVG -DPGB_HIGH_LCD_ACCURACY=1 (the first, slow version).
  CORE="$CORE $PG/src/pgb_frontend.c|${GB_OPT:--O2}${GB_FLAGS:+@$GB_FLAGS} $O/rom_data.c"
  FLAGS="$FLAGS -I$PG/src -I$PG/out/peanut-gb-patched -I$AB/include"
  EXTRA_CXX="$AB/src/frl_core.cpp|;$HERE/examples/snake/frl_edumkii.cpp|;$HERE/examples/gameboy/gb_app.cpp|"
  CXXF="-I$AB/include -I$PG/src"
  [ -n "$AUTOSTART" ] && CXXF="$CXXF -DEDU_GB_SCRIPT"
  [ -n "$GB_UNPACED" ] && CXXF="$CXXF -DEDU_GB_UNPACED"
elif [ "$APP" = llama2c ]; then
  # TinyStories 260K int8 (ports/llama2c) writing on the screen. Needs numpy for the quantizer: PYTHON=<python3 with numpy>.
  LL="$REPO/ports/llama2c"; MODEL="${MODEL:-$LL/model-cache}"; GS=4
  bash "$LL/fetch.sh" "$MODEL"
  cp "$HERE/examples/llama2c/llama2c.c" "$O/$EX.c"
  ( cd "$O" && "${PYTHON:-python3}" -I "$LL/tools/quantize_q80.py" "$MODEL/stories260K.bin" q80.bin $GS && python3 -I "$LL/tools/embed.py" q80.bin "$MODEL/tok512.bin" blobs.c )
  sed -i '' -e "s/--stack_size=[0-9]*/--stack_size=4096/" "$O/ticlang/device_linker.cmd"
  CORE="$CORE $LL/src/llama2q.c|-O2@-include@$LL/target-m33/cycles.h@-DLL_STEPS=128 $O/blobs.c"
  FLAGS="$FLAGS -I$LL/src"
  CXXF=""
elif [ -n "$APP_HOOK" ]; then
  # An app that lives in another folder: APP_HOOK=<script> is sourced here. It sees HERE REPO O EX FLAGS CORE EXTRA_CXX CXXF,
  # copies its main to "$O/$EX.c" and may add sources to CORE ("path|flag@flag") and EXTRA_CXX ("path|flags;"), flags to FLAGS and CXXF.
  . "$APP_HOOK"
else
  cp "$HERE/examples/$APP/$APP.c" "$O/$EX.c"
  CXXF=""
fi
python3 - "$O/ticlang/makefile" "$FLAGS" "$CORE" "$EXTRA_CXX" "$CXXF" <<'PY'
import sys, re, os
mk, flags, core, extra, cxxf = sys.argv[1], sys.argv[2], sys.argv[3].split(), sys.argv[4], sys.argv[5]
cflag = {}                                   # a C source can carry its own flags: "path|flag@flag" (@ stands for a space)
for i, e in enumerate(core):
    if "|" in e:
        pth, fl = e.split("|", 1); core[i] = pth; cflag[pth] = fl.replace("@", " ")
t = open(mk).read()
cxx = [e.split("|", 1) for e in extra.split(";") if e]
base = lambda p: os.path.splitext(os.path.basename(p))[0]
objs = " ".join(base(s) + ".obj" for s in core) + " " + " ".join(base(p) + ".obj" for p, _ in cxx)
rules = "\n".join("%s.obj: %s $(SYSCFG_H_FILES)\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) %s -c $< -o $@\n" % (base(s), s, cflag.get(s, "")) for s in core)
CXXBASE = "-std=gnu++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics " + cxxf
rules += "\n".join("%s.obj: %s $(SYSCFG_H_FILES)\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) %s %s -c $< -o $@\n" % (base(p), p, CXXBASE, x) for p, x in cxx)
t = re.sub(r"^(OBJECTS = .*)$", lambda m: m.group(1) + " " + objs, t, count=1, flags=re.M)
t += "\nCFLAGS += %s -ffunction-sections -fdata-sections -Wno-unused-function -w\n" % flags
t = t.replace("$(NAME).out: $(OBJECTS)", rules + "\n$(NAME).out: $(OBJECTS)", 1)
open(mk, "w").write(t)
PY
# SysConfig writes its files beside the project: the generated board_config.h is found through -I gen
MK="make -j${EDU_JOBS:-1} -C $O/ticlang MSPM0_SDK_INSTALL_DIR=$SDK MSPM33_SDK_INSTALL_DIR=$SDK SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CC GCC_ARMCOMPILER="
$MK > "$O/build.log" 2>&1 || { tail -25 "$O/build.log"; exit 1; }
OUTF=$(ls "$O"/ticlang/*.out)
python3 - "$O"/ticlang/*.map "$TAG" <<'PY'
import sys
m = {}
for l in open(sys.argv[1]):
    p = l.split()
    if len(p) >= 4 and (p[0] == "FLASH" or p[0].startswith("SRAM")):
        k = "FLASH" if p[0] == "FLASH" else "SRAM"
        a, b = m.get(k, (0, 0))
        m[k] = (a + int(p[3], 16), b + int(p[2], 16))
print("%s: flash %d of %d B, RAM %d of %d B (from the map; RAM excludes the stack reserve)" % (sys.argv[2], m["FLASH"][0], m["FLASH"][1], m["SRAM"][0], m["SRAM"][1]))
PY
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
rm -f "$O"/ticlang/*.obj "$O"/Snake.ino.cpp.bak
echo "image: $OUTF"
