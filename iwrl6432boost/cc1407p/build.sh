#!/bin/bash
# Build the CC1407P host for the IWRL6432BOOST on a Mac: the SDK's "empty" no-RTOS example folder with this
# repo's sources, TI's CC140xP host driver (third-party/ti-cc140xp-host) and the bare HAL (portability/cc1407p-host).
# Usage: build.sh [work dir]   (default <scratch>/radar-cc1407p-build). Result: <work>/radio_tx/.../ti-arm-clang/radio_tx.release.out
# Needs MMWAVE-L-SDK 05.05.04.02 in ~/ti, TI Arm Clang and SysConfig 1.28.0 as in portability/radar-family/build/build.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
B=${MMWAVE_L_SDK:-$HOME/ti/mmwave_l_sdk_05_05_04_02}
W="${1:-$HOME/tmp/_agent/radar-cc1407p-build}"; APP=${APP:-radio_tx}
T=$REPO/third-party/ti-cc140xp-host; P=$REPO/portability/cc1407p-host
rm -rf "$W/$APP"; mkdir -p "$W"; cp -R "$B/examples/empty" "$W/$APP"
D="$W/$APP/xwrL64xx-evm/m4fss0-0_nortos"; M="$D/ti-arm-clang/makefile"
rm -f "$D"/ti-arm-clang/*.out "$D"/ti-arm-clang/*.appimage "$D"/ti-arm-clang/*.map
cp "$HERE/example.syscfg" "$D/example.syscfg"
sed -i.bak "s/empty_main/${APP}_main/g" "$D/main.c"; rm -f "$D/main.c.bak"
cp "$HERE/$APP.c" "$HERE/hooks_iwrl6432.c" "$W/$APP/"
SET="$T/rcl_settings_2gfsk50kbps.c"
EXTRA="$SET $T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c $P/hal_bare.c"
python3 - "$M" "$EXTRA" "$T" "$P" "$APP" <<'PY'
import sys, re, os
m, extra, t, p, app = sys.argv[1:]
s = open(m).read()
s = s.replace("empty.release", app + ".release").replace("empty.c", app + ".c \\\n\thooks_iwrl6432.c")
s = s.replace("-Werror", "-w")
s = s.replace("INCLUDES_common := \\\n", f"INCLUDES_common := \\\n\t-I{t}/host -I{t}/settings -I{t} -I{p} -I{p}/shim \\\n", 1)
if "$(PROFILE)" in s and "OUTNAME:=empty" in s: s = s.replace("OUTNAME:=empty", "OUTNAME:=" + app)
# extra sources: add to FILES and their directories to the search path
srcs = extra.split()
files = "".join(f"\t{os.path.basename(c)} \\\n" for c in srcs)
paths = "".join(f"\t{os.path.dirname(c)} \\\n" for c in srcs)
s = s.replace("FILES_common := \\\n", "FILES_common := \\\n" + files, 1)
s = s.replace("FILES_PATH_common = \\\n", "FILES_PATH_common = \\\n" + paths + f"\t../../../.. \\\n", 1)
open(m, "w").write(s)
PY
cd "$D/ti-arm-clang"
make -j8 $APP.release.out MCU_PLUS_SDK_PATH="$B" PROFILE=release TOOLS_PATH="$HOME/ti" CCS_PATH=/Applications/ti/ccs2101/ccs \
  CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0 \
  "MKDIR=mkdir -p" "RMDIR=rm -rf" "RM=rm -f" COPY=cp CAT=cat TOUCH=touch > "$W/$APP.build.log" 2>&1 || { tail -30 "$W/$APP.build.log"; exit 1; }
ls -l $APP.release.out
/Applications/ti/ti-cgt-armllvm_release/bin/tiarmsize $APP.release.out
