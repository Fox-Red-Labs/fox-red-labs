#!/bin/bash
# Build the BOOSTXL-AUDIO app for the LP-MSPM0G3519 with TI's SDK makefile (the gpio_toggle_output example as the frame). Does not flash.
#   ./build.sh [variant]   variant: default or autoplay        Output: out/boosterpack-speedrun/g3519-audio-<variant>/
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
V="${1:-default}"; E=gpio_toggle_output
: "${FRL_SYSCONFIG:=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh}"; export FRL_SYSCONFIG   # 1.26.3 is the validated version and is not installed here; 1.28.0 is used and the run is unvalidated
. "$ROOT/../../../tools/frl/toolpaths.sh"
W="${OUT:-$PWD/out/boosterpack-speedrun/g3519-audio-$V}"; rm -rf "$W"; mkdir -p "$W"
cp -R "$SDK/examples/nortos/LP_MSPM0G3519/driverlib/$E" "$W/"
cp "$HERE/board.c" "$W/$E/$E.c"; cp "$HERE/audio.syscfg" "$W/$E/$E.syscfg"
rm -f "$W/$E/ti_msp_dl_config.c" "$W/$E/ti_msp_dl_config.h"
DEF=""; [ "$V" = autoplay ] && DEF="-DAUTOPLAY"
M="$W/$E/ticlang/makefile"
python3 - "$M" "$DEF -I$ROOT/core -I$ROOT/songs -I$ROOT/app" "$ROOT/app/audio_app.c $ROOT/core/ba_dac8311.c $ROOT/core/ba_tone.c $ROOT/core/ba_chip.c" <<'PY'
import sys
m, cflags, srcs = sys.argv[1:]
s = open(m).read()
import os
srcl = srcs.split()
s = s.replace("OBJECTS = gpio_toggle_output.obj", "EXTRA_SRCS = " + " ".join(srcl) + "\nOBJECTS = gpio_toggle_output.obj " + " ".join(os.path.basename(x).replace('.c', '.obj') for x in srcl))
s = s.replace("CFLAGS += -I.. \\", "CFLAGS += " + cflags + " \\\n    -I.. \\")
s = s.replace("\ngpio_toggle_output.obj: ../gpio_toggle_output.c", "\n$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))\n\ngpio_toggle_output.obj: ../gpio_toggle_output.c", 1)
open(m, "w").write(s)
PY
"$ROOT/../../../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$W/$E/ticlang" > "$W/build.log" 2>&1 || { tail -30 "$W/build.log"; exit 1; }
echo "built $W/$E/ticlang/$E.out"; grep -E "^ +(FLASH|SRAM|RAM)" "$W/$E/ticlang/$E.map" | head -4
