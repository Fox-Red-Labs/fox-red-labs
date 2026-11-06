#!/bin/bash
# Control-loop bench (bench/ctrl) for the LP-MSPM33C321A: TI's hsadc_triggered_by_timer_event with our syscfg and main.c.
#   build_ctrl.sh [O2|O3] [main.c|main_fom.c]   output <run>/ctrl-<opt>[-fom]/ticlang/hsadc_triggered_by_timer_event.out
set -e
OPT=${1:-O2}; SRC=${2:-main.c}; SUF=; [ "$SRC" = main_fom.c ] && SUF=-fom; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/mspm33c321a}"; O="$R/ctrl-$OPT$SUF"; EX=hsadc_triggered_by_timer_event
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
[ "$(shasum "$HERE/bench/ctrl/foc.c" | cut -c1-40)" = "$(shasum "$HERE/../c2000-launchpad/bench/ctrl/foc.c" | cut -c1-40)" ] || { echo "foc.c differs from the C2000's"; exit 1; }
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$O"
cp "$HERE/bench/ctrl/ctrl.syscfg" "$O/$EX.syscfg"; cp "$HERE/bench/ctrl/$SRC" "$O/$EX.c"; cp "$HERE/bench/ctrl/foc.c" "$HERE/bench/ctrl/foc.h" "$HERE/bench/m33_timer.h" "$HERE/../portability/fom/fom.h" "$O/"
sed -i '' "s/-O2/-$OPT/" "$O/ticlang/makefile"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "ctrl $OPT built" || { tail -15 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
