#!/bin/bash
# One command for the haptic knob on the LP-AM13E230 + DRV8376EVM + Anaheim BLY17. Run from the repo root:
#   am13e230/haptic-knob/run.sh            preflight, build, flash the commissioning image, serve the page
# The motor is NOT energised by this script: the image boots with DRVOFF high and all PWM low. The run starts only when you
# press Start in the page (or run knob_cli.py), with the 12 V supply on and your hands clear.
set -e
cd "$(dirname "$0")/../.."
SERIAL=${PROBE:-AL130001}
am13e230/foc-drv8376/build.sh
OUT=am13e230/foc-drv8376/out/am13e230/foc-drv8376
python3 tools/preflight_image.py am13e230 $OUT/foc.out --map $OUT/foc.map
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
$DSLITE flash -c am13e230/AM13E23019_AL130001.ccxml -e -f -v -r 1 -u -t 60 $OUT/foc.out
echo "flashed. Open http://localhost:8000/ (Chrome), connect the serial port, tick the two boxes, press Start."
cd am13e230/haptic-knob/web && python3 -m http.server 8000
