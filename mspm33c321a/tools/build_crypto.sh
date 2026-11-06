#!/bin/bash
# TI's crypto/<example> (MSP Crypto Library 0.33) with one DWT cycle count around its single crypto call.
#   build_crypto.sh <example> [dir]    output <dir>/crypto-<example>/ticlang/<example>.out ; RAM struct `xr`
# The count runs from the start of main (after SysConfig init) to just before the example's own status check,
# one cold call, code in flash. xr.pass is 1 when the example reached its pass trap (its own ACVP/CAVP check).
E=$1; R="${2:-$PWD/out/mspm33c321a}"; O="$R/crypto-$E"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/crypto/$E" "$O"
python3 - "$O/$E.c" <<'P'
import sys,re
p=sys.argv[1]; s=open(p).read()
s=s.replace('int main(void)','volatile struct { uint32_t magic, cycles, pass, cpu_hz; } xr;\nint main(void)',1)
s=s.replace('SYSCFG_DL_init();','''SYSCFG_DL_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t t0 = DWT->CYCCNT;''',1)
import os
if os.environ.get('LATE'):
    i=s.index('/* Trap here on a pass condition */')
    s=s[:i]+'xr.cycles = DWT->CYCCNT - t0;\n    '+s[i:]
else:
    i=s.index('if (stat')
    s=s[:i]+'xr.cycles = DWT->CYCCNT - t0;\n    '+s[i:]
j=s.index('/* Trap here on a pass condition */')
s=s[:j]+'xr.pass = 1; xr.cpu_hz = CPUCLK_FREQ; xr.magic = 0x5EC00001u;\n    '+s[j:]
k=s.index('err:')
k2=s.index('/* Trap here on an error condition */')
s=s[:k2]+'xr.pass = 0; xr.cpu_hz = CPUCLK_FREQ; xr.magic = 0x5EC00001u;\n    '+s[k2:]
open(p,'w').write(s)
P
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "$E built" || { echo "$E FAILED"; tail -5 "$O/build.log"; exit 1; }
