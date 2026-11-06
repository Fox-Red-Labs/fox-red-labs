#!/bin/bash
# Build the BOOSTXL-AUDIO app for the LP-MSPM33C321A with TI's SDK makefile (the gpio_toggle_output example as the frame). Does not flash.
#   ./build.sh [variant]   variant: default, autoplay, algos or halloween (the show: main_halloween.c, halloween.syscfg)        Output: out/boosterpack-speedrun/m33c321a-audio-<variant>/
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
V="${1:-default}"; E=gpio_toggle_output
TOOLPATHS_FAMILY=mspm33-cc; . "$ROOT/../../../tools/frl/toolpaths.sh"   # SDK = MSPM33 SDK, CC = TI Arm Clang
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
W="${OUT:-$PWD/out/boosterpack-speedrun/m33c321a-audio-$V}"; rm -rf "$W"; mkdir -p "$W"
cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$E" "$W/"
BOARD="$HERE/board.c"; SC="$HERE/audio.syscfg"; [ "$V" = halloween ] && { BOARD="$HERE/main_halloween.c"; SC="$HERE/halloween.syscfg"; }
cp "$BOARD" "$W/$E/$E.c"; cp "$SC" "$W/$E/$E.syscfg"
if [ "$V" = halloween ]; then   # sample rate (default 16 kHz) sets the SysTick period at 160 MHz; recorded tracks from both folders
  RATE="${RATE:-16000}"; sed -i "" "s/SYSTICK.period          = 20000;/SYSTICK.period          = $((160000000 / RATE));/" "$W/$E/$E.syscfg"
  python3 "$ROOT/tools/tracks2c.py" "$W/tracks_gen.c" "$ROOT/tracks" "$PWD/out/halloween-tracks"
fi
rm -f "$W/$E/ti_msp_dl_config.c" "$W/$E/ti_msp_dl_config.h"
DEF=""; [ "$V" = autoplay ] && DEF="-DAUTOPLAY"; [ "$V" = halloween ] && DEF="-DSAMPLE_RATE=${RATE}u"
SRCS="$ROOT/app/audio_app.c $ROOT/core/ba_dac8311.c $ROOT/core/ba_tone.c $ROOT/core/ba_chip.c"
if [ "$V" = halloween ]; then SRCS="$W/tracks_gen.c $ROOT/app/halloween_app.c $ROOT/core/ba_dac8311.c $ROOT/core/ba_chip.c $ROOT/core/ba_tone.c $ROOT/algos/ba_yin.c $(ls $ROOT/halloween/*.c | tr '\n' ' ')"; fi
if [ "$V" = algos ]; then SRCS="$ROOT/app/algos_app.c $ROOT/core/ba_dac8311.c $ROOT/core/ba_tone.c $(ls $ROOT/algos/*.c | tr '\n' ' ')"; fi
M="$W/$E/ticlang/makefile"
[ "$V" = halloween ] && export STACK=4096   # the show and its listener need more than the 256 B default stack
python3 - "$M" "$DEF -I$ROOT/core -I$ROOT/songs -I$ROOT/app -I$ROOT/algos -I$ROOT/halloween" "$SRCS" <<'PY'
import sys
m, cflags, srcs = sys.argv[1:]
s = open(m).read()
import os
srcl = srcs.split()
s = s.replace("OBJECTS = gpio_toggle_output.obj", "EXTRA_SRCS = " + " ".join(srcl) + "\nOBJECTS = gpio_toggle_output.obj " + " ".join(os.path.basename(x).replace('.c', '.obj') for x in srcl))
s = s.replace("CFLAGS += -I.. \\", "CFLAGS += " + cflags + " \\\n    -I.. \\")
if os.environ.get("STACK"): s = s.replace("    -Wl,--rom_model \\", "    -Wl,--rom_model \\\n    -Wl,--stack_size=" + os.environ["STACK"] + " \\")
s = s.replace("\ngpio_toggle_output.obj: ../gpio_toggle_output.c", "\n$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))\n\ngpio_toggle_output.obj: ../gpio_toggle_output.c", 1)
open(m, "w").write(s)
PY
make -C "$W/$E/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$W/build.log" 2>&1 || { tail -30 "$W/build.log"; exit 1; }
echo "built $W/$E/ticlang/$E.out"; grep -E "^ +(FLASH|SRAM|RAM)" "$W/$E/ticlang/$E.map" | head -4
