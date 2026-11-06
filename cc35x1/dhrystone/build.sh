#!/bin/bash
# Dhrystone 2.1 for the LP-EM-CC35X1. ./build.sh <O3|Os|O3i> [runs]  -> out/cc35x1/dhry/<variant>
#   O3  -O3 -fno-inline (follows Weicker's rules)   Os  -Os -fno-inline   O3i  -O3 with inlining (not rules-compliant; experiment)
set -u
V=$1; RUNS=${2:-4000000}
case $V in O3) OPT="-O3 -fno-inline";; Os) OPT="-Os -fno-inline";; O3i) OPT="-O3";; *) echo "O3|Os|O3i"; exit 1;; esac
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"; RUN="$PWD/out/cc35x1/dhry"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
S="$REPO/mspm0g3507/mspm0l1306/bench/dhrystone/src"; S="$REPO/mspm0l1306/bench/dhrystone/src"
C="$RUN/$V"; rm -rf "$C"; mkdir -p "$RUN"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo" "$C"
cp "$HERE/dhry_run.c" "$C/uart2echo.c"; mkdir -p "$C/psrc"; cp "$S/dhry.h" "$S/dhry_1.c" "$S/dhry_2.c" "$C/psrc/"
sed -i '' 's/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE 4096/' "$C/freertos/main_freertos.c"
M="$C/freertos/ticlang/makefile"
sed -i '' -e "s#^OBJECTS = uart2echo.obj#OBJECTS = uart2echo.obj dhry_1.obj dhry_2.obj#" -e "s#^    -Oz \\\\#    -O2 -I$C/psrc -DTIME -DDH_RUNS=$RUNS \\\\#" "$M"
cat >> "$M" <<MAKE

vpath %.c $C/psrc
dhry_%.obj: dhry_%.c \$(SYSCFG_H_FILES)
	\$(CC) \$(CFLAGS) $OPT -std=gnu89 -w -DTIME -include string.h -Dmain=dhry_main -Dprintf=dh_printf -Dscanf=dh_scanf -Dmalloc=dh_malloc -c \$< -o \$@
MAKE
make -C "$C/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/$V.log" 2>&1 \
	&& echo "$V ok: $C/freertos/ticlang/uart2echo.out" || { echo "$V FAILED"; grep -m3 -i "error" "$RUN/$V.log"; }
