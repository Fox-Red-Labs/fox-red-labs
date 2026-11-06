#!/bin/bash
# Build the sleepy Zigbee temperature sensor for the LP-CC1354P10-1 from TI's zed_temperaturesensor example
# (SimpleLink CC13xx CC26xx SDK 8.33, Z-Stack, TI-RTOS 7), from the terminal.
#   ./build.sh            (needs the F2 SDK and CCStudio 20.x at the paths below)
# TI ships no makefile for Zigbee examples on this board, only a CCStudio project (.projectspec), so the
# build imports it with CCStudio's headless command line into a fresh workspace and builds it there.
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16}
CCS_CLI=${CCS_CLI:-/Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh}
HERE="$(cd "$(dirname "$0")" && pwd)"
EX=$SDK/examples/rtos/LP_EM_CC1354P10_1/zstack/zed_temperaturesensor
APP=$SDK/source/ti/zstack/apps/temperaturesensor
NAME=zed_temperaturesensor_LP_EM_CC1354P10_1_tirtos7_ticlang

# Fresh copy of TI's example and the three app files it links from the SDK (cp, not rsync -a:
# the SDK folders carry a setgid bit rsync cannot copy here). Then our changes, as one patch.
rm -rf "$HERE/zed_temperaturesensor" "$HERE/workspace"
cp -R "$EX" "$HERE/zed_temperaturesensor"
mkdir -p "$HERE/zed_temperaturesensor/app"
cp "$APP/zcl_sampletemperaturesensor.c" "$APP/zcl_sampletemperaturesensor.h" "$APP/zcl_sampletemperaturesensor_data.c" \
   "$HERE/zed_temperaturesensor/app/"
chmod -R u+w "$HERE/zed_temperaturesensor"
( cd "$HERE/zed_temperaturesensor" && patch -p1 -s < "$HERE/sleepy_sensor.patch" )

# Import and build. ELECTRON_RUN_AS_NODE (set in VS Code terminals) is removed so CCStudio's tools start normally.
env -u ELECTRON_RUN_AS_NODE "$CCS_CLI" -noSplash -workspace "$HERE/workspace" -application projectImport \
    -ccs.location "$HERE/zed_temperaturesensor/tirtos7/ticlang/$NAME.projectspec" </dev/null
env -u ELECTRON_RUN_AS_NODE "$CCS_CLI" -noSplash -workspace "$HERE/workspace" -application projectBuild \
    -ccs.projects "$NAME" -ccs.configuration default </dev/null
# The build summary says "0 out of 1 projects have errors" even when SysConfig failed: check the image exists.
OUT="$HERE/workspace/$NAME/default/$NAME.out"
test -f "$OUT" || { echo "no image built: read the build output above"; exit 1; }
mkdir -p "$HERE/out"
cp "$OUT" "$HERE/out/frl_sleepy_sensor.out"
cp "$HERE/workspace/$NAME/default/$NAME.map" "$HERE/out/frl_sleepy_sensor.map"
grep -A8 "MEMORY CONFIGURATION" "$HERE/out/frl_sleepy_sensor.map" | grep -E " FLASH | SRAM | CCFG "
echo "built: $HERE/out/frl_sleepy_sensor.out"
echo "load and run: /Applications/ti/ccs2101/ccs/scripting/python/run.sh $HERE/../tools/ccs_load_run.py $HERE/out/frl_sleepy_sensor.out"
