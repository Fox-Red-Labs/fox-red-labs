#!/bin/bash
# Three ways, TI Drivers on the LP-EM-CC35X1 (Wi-Fi SDK, FreeRTOS). Usage: ./build_cc35x1.sh [outdir]
# Copies the SDK's drivers/empty example, swaps in this folder's syscfg and glue, adds common/app.c. Output: <outdir>/ (default build-cc35x1/, git-ignored).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; O="${1:-$HERE/build-cc35x1}"; LOG="$HERE/../logs/tidrivers-cc35x1.log"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$O"; mkdir -p "$O"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/empty" "$O/e"; rm -rf "$O/e/freertos/gcc"
cp "$HERE/cc35x1/glue_tidrivers_cc35x1.c" "$O/e/empty.c"; cp "$HERE/cc35x1/app.syscfg" "$O/e/freertos/empty.syscfg"
cp "$HERE/../common/app.c" "$HERE/../common/app.h" "$O/e/"
M="$O/e/freertos/ticlang/makefile"
sed -i '' 's/^OBJECTS = empty.obj /OBJECTS = empty.obj app.obj /' "$M"
python3 - "$M" <<'PY'
import sys,re
p=sys.argv[1]; t=open(p).read()
m=re.search(r"empty\.obj: \.\./\.\./empty\.c.*?\n(\t.*\n)+", t)
blk=m.group(0); new=blk.replace("empty.obj","app.obj").replace("../../empty.c","../../app.c")
t=t.replace(blk, blk+"\n"+new); open(p,'w').write(t)
PY
t0=$(python3 -c 'import time;print(time.time())')
make -C "$O/e/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$LOG" 2>&1 || { tail -25 "$LOG"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
echo "== clean build $(python3 -c "print(round($t1-$t0,1))") s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CGT/bin/tiarmsize" "$O/e/freertos/ticlang/empty.out" | tee -a "$LOG"
cp "$O/e/freertos/ticlang/empty.map" "$HERE/../maps/tidrivers-cc35x1.map" 2>/dev/null || true
