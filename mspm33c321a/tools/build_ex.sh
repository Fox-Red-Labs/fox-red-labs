#!/bin/bash
# Build a bench that needs a TI example's SysConfig module: copy the SDK example, put our main.c over it.
#   build_ex.sh <sdk example dir under driverlib> <bench name> [extra source files] ; EXTRAINC / EXTRAFLAGS env as build_bench.sh
#   output <dir>/<bench>/ticlang/<example>.out
EX=$1; N=$2; shift 2; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/mspm33c321a}"; O="$R/$N"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$O"; python3 -c "
import sys
t=open(sys.argv[1]).read().replace('\"../m33_timer.h\"','\"m33_timer.h\"')
t=t.replace('#include \"ti_msp_dl_config.h\"','#include \"ti_msp_dl_config.h\"\n#ifndef CPU_HZ\n#define CPU_HZ CPUCLK_FREQ\n#endif',1)
open(sys.argv[2],'w').write(t)" "$HERE/bench/$N/main.c" "$O/$EX.c"; cp "$HERE/bench/m33_timer.h" "$O/"
for f in "$@"; do cp "$f" "$O/"; done
if [ -n "$EXTRAMK" ]; then :; fi
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "$N built" || { tail -8 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
