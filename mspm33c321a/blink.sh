#!/bin/bash
# One-command blink for the LP-MSPM33C321A: TI's driverlib/gpio_toggle_output (RGB LED: blue PA2, red PC26,
# green PC27) with our own blink period, built from the terminal by TI's own makefile and flashed over the
# onboard XDS110 with UniFlash's DSLite. No CCStudio needed.
#
#   ./blink.sh [ms] [probe-serial]      default 250 ms (TI's example uses 500 at 32 MHz)
#
# Builds into $OUT/blink-<ms>/ ($OUT defaults to ./build-mspm33c321a in the folder you run it from), prints build and flash seconds.
# Needs: TI Arm Clang 5.1.1.LTS (comes with CCStudio 21.0.1), SysConfig 1.28.0, MSPM33 SDK 1.04.00.00, UniFlash 9.6.0.
set -u
MS="${1:-250}"; SER="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-$PWD/build-mspm33c321a}/blink-$MS"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
EX="$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output"
if [ -z "$SER" ]; then SER=$(ls "$HERE"/MSPM33C321A_*.ccxml 2>/dev/null | head -1 | sed 's/.*_\(.*\)\.ccxml/\1/'); fi
CCXML="$HERE/MSPM33C321A_$SER.ccxml"
[ -f "$CCXML" ] || { echo "No $CCXML. Run tools/identify.sh <probe-serial> first."; exit 2; }
CYC=$(( MS * 32000 ))
rm -rf "$OUT"; cp -R "$EX" "$OUT"
sed -i '' "s/#define DELAY (16000000)/#define DELAY ($CYC)/" "$OUT/gpio_toggle_output.c"
grep -q "DELAY ($CYC)" "$OUT/gpio_toggle_output.c" || { echo "Could not change the delay in the example"; exit 1; }
t0=$(python3 -c 'import time;print(time.time())')
make -C "$OUT/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$OUT/build.log" 2>&1 \
  || { echo "Build failed: $OUT/build.log"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
"$DSLITE" flash -c "$CCXML" -e -f -v "$OUT/ticlang/gpio_toggle_output.out" > "$OUT/flash.log" 2>&1 \
  || { echo "Flash failed: $OUT/flash.log"; tail -5 "$OUT/flash.log"; exit 1; }
t2=$(python3 -c 'import time;print(time.time())')
python3 -c "print(f'build {$t1-$t0:.1f} s, flash {$t2-$t1:.1f} s, LEDs toggle every ${MS} ms')"
