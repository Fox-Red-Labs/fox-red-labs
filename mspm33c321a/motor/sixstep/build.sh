#!/bin/bash
# DRV8376EVM six-step spin for the LP-MSPM33C321A. Build only.  Output: $OUT/sixstep/ticlang/*.out
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="${OUT:-$PWD/out/mspm33c321a}/sixstep"; EX=hsadc_conversion_software_trigger
TOOLPATHS_FAMILY=mspm33-cc; . "$HERE/../../../tools/frl/toolpaths.sh"
rm -rf "$OUT"; mkdir -p "$(dirname "$OUT")"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$OUT"
cp "$HERE/sixstep.syscfg" "$OUT/$EX.syscfg"; cp "$HERE/sixstep.c" "$OUT/$EX.c"
[ -n "${TEST_BURST6:-}" ] && sed -i '' "s/^#ifndef ON_CYC/#define TEST_BURST6 1\n#ifndef ON_CYC/" "$OUT/$EX.c"
[ -n "${TEST_BURST:-}" ] && sed -i '' "s/^#ifndef ON_CYC/#define TEST_BURST 1\n#ifndef ON_CYC/" "$OUT/$EX.c"
[ -n "${TEST_PULSE:-}" ] && sed -i '' "s/^#ifndef ON_CYC/#define TEST_PULSE 1\n#ifndef ON_CYC/" "$OUT/$EX.c"
[ -n "${ON_CYC:-}" ] && sed -i '' "s/^#ifndef ON_CYC/#define ON_CYC ${ON_CYC}u\n#ifndef ON_CYC/" "$OUT/$EX.c"
make -C "$OUT/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$OUT/build.log" 2>&1 || { grep -iE "error|warning" "$OUT/build.log" | head -20; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$OUT"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}'
echo "built $OUT/ticlang/$EX.out"
