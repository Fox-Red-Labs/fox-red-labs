#!/bin/bash
# Digital-power PID loop (bench/power-loop/main_pl.c) for the LP-MSPM33C321A: TI's hsadc_triggered_by_timer_event with the control bench's syscfg.
#   build_pl.sh [O2|O3]   output <run>/pl-<opt>/ticlang/hsadc_triggered_by_timer_event.out  (map beside it; symbols fom and pl)
set -e
OPT=${1:-O2}; SUF=; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/speed-run/mspm33c321a}"; O="$R/pl-$OPT"; EX=hsadc_triggered_by_timer_event
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
[ "$(shasum "$HERE/../cc1354p10/bench/power-loop/pid.c" | cut -c1-40)" = "$(shasum "$HERE/../c2000-launchpad/bench/power-loop/pid.c" | cut -c1-40)" ] || { echo "pid.c differs from the C2000's"; exit 1; }
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$O"
cp "$HERE/bench/ctrl/ctrl.syscfg" "$O/$EX.syscfg"; cp "$HERE/bench/power-loop/main_pl.c" "$O/$EX.c"; cp "$HERE/../cc1354p10/bench/power-loop/pid.c" "$HERE/../cc1354p10/bench/power-loop/pid.h" "$HERE/bench/m33_timer.h" "$HERE/../portability/fom/fom.h" "$O/"
sed -i '' "s/-O2/-$OPT/" "$O/ticlang/makefile"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "pl $OPT built" || { tail -15 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
