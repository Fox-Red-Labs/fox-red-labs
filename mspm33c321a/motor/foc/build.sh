#!/bin/bash
# Hall FOC for the LP-MSPM33C321A + DRV8376EVM. Build only. SRC=probe.c for a SysConfig check. TEST_* select bench tests.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="${OUT:-$PWD/out/mspm33c321a}/foc"; EX=hsadc_conversion_software_trigger
TOOLPATHS_FAMILY=mspm33-cc; . "$HERE/../../../tools/frl/toolpaths.sh"
rm -rf "$OUT"; mkdir -p "$(dirname "$OUT")"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$OUT"
cp "$HERE/foc.syscfg" "$OUT/$EX.syscfg"; cp "$HERE/${SRC:-foc.c}" "$OUT/$EX.c"
CORE="$HERE/../../../portability/motor/core"
for t in ${TESTS:-}; do sed -i '' "1s/^/#define $t 1\n/" "$OUT/$EX.c"; done
[ -n "${TRIG:-}" ] && sed -i '' "1s/^/#define FRL_TRIG FRL_TRIG_${TRIG}\n/" "$OUT/$EX.c"
sed -i "" "s|^CFLAGS += -I.. \\\\|CFLAGS += -I.. -I$CORE \\\\|" "$OUT/ticlang/makefile"
make -C "$OUT/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$OUT/build.log" 2>&1 || { grep -iE "error" "$OUT/build.log" | head -20; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$OUT"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}'
echo "built $OUT/ticlang/$EX.out"
