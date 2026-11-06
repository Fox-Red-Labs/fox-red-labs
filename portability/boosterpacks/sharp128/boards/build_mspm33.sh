#!/bin/bash
# Build a Sharp128 example for the LP-MSPM33C321A with TI Arm Clang, nothing flashed.
#   boards/build_mspm33.sh <hello|player>
# TI's makefile for the gpio_toggle_output example does the SysConfig and link; this script copies that example,
# swaps in our SysConfig file, and makes the example's one source file include ours (one translation unit).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; S="$HERE/.."; ROOT="$HERE/../../../.."
APP="${1:-hello}"
OUT="${OUT:-$PWD/out/boosterpack-speedrun/sharp128}"; W="$OUT/mspm33c321a/$APP"
SYSCFG="${FRL_SYSCONFIG:-}"
if [ -z "$SYSCFG" ]; then for c in /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh /Applications/ti/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh; do [ -x "$c" ] && SYSCFG="$c" && break; done; fi
export FRL_SYSCONFIG="$SYSCFG"
TOOLPATHS_FAMILY=mspm33-cc; . "$ROOT/tools/frl/toolpaths.sh"
rm -rf "$W"; mkdir -p "$W"
cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output" "$W/x"
cp "$HERE/mspm33c321a/sharp128.syscfg" "$W/x/gpio_toggle_output.syscfg"
STG="$W/stage"; mkdir -p "$STG"
for f in ff.c ff.h diskio.h ffunicode.c; do cp "$ROOT/third-party/fatfs/$f" "$STG/"; done; cp "$S/ffconf.h" "$STG/"
case "$APP" in hello) MAIN="$S/examples/hello/hello.c";; player) MAIN="$S/player/player.c";; *) echo bad app; exit 2;; esac
cat > "$W/x/gpio_toggle_output.c" <<UNITY
/* one translation unit: the example's makefile builds only this file */
#include "$S/sharp_lcd.c"
#include "$S/sharp_gfx.c"
#include "$S/sharp_sd.c"
#include "$STG/ff.c"
#include "$STG/ffunicode.c"
#include "$HERE/mspm0_common/port_mspm0.c"
#include "$MAIN"
UNITY
make -C "$W/x/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= \
  DFLAGS="-I$S -I$HERE/mspm0_common -I$STG -DSHARP_PORT_HAS_SD -DSHARP_WITH_FATFS -DBOARD_NAME=\\\"LP-MSPM33C321A\\\"" > "$W/build.log" 2>&1 \
  || { grep -E "error" "$W/build.log" | head -10; exit 1; }
cp "$W/x/ticlang/gpio_toggle_output.out" "$W/$APP.out"; cp "$W"/x/ticlang/*.map "$W/$APP.map" 2>/dev/null || true
echo "mspm33c321a $APP:"; "$CC/bin/tiarmsize" "$W/$APP.out" | tail -1
