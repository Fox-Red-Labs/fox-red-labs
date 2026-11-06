#!/bin/bash
# Build the occupancy-model harness (occ_run.c) for the LP-EM-CC35X1 at -Os and -O3, into out/cc35x1/occ/<opt>.
# Copies TI's drivers/uart2echo, swaps in the harness and the four model sources, builds with the SDK makefile.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"; RUN="$PWD/out/cc35x1/occ"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
U="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"
for o in Os O3; do
	C="$RUN/$o"; rm -rf "$C"; mkdir -p "$RUN"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo" "$C"
	cp "$HERE/occ_run.c" "$C/uart2echo.c"
	sed -i '' 's/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE 4096/' "$C/freertos/main_freertos.c"
	M="$C/freertos/ticlang/makefile"
	sed -i '' -e "s#^OBJECTS = uart2echo.obj#OBJECTS = uart2echo.obj occ_cnn_500.obj occ_cnn_2k.obj occ_cnn_8k.obj occ_cnn_30k.obj#" \
		-e "s#^    -Oz \\\\#    -$o -ffp-contract=off -I$U \\\\#" "$M"
	cat >> "$M" <<MAKE

vpath %.c $U/occ_cnn_500 $U/occ_cnn_2k $U/occ_cnn_8k $U/occ_cnn_30k
occ_%.obj: occ_%.c \$(SYSCFG_H_FILES)
	\$(CC) \$(CFLAGS) -I$U/\$* -c \$< -o \$@
MAKE
	make -C "$C/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/$o.log" 2>&1 \
		&& echo "$o ok: $C/freertos/ticlang/uart2echo.out" || { echo "$o FAILED"; grep -m3 -i "error" "$RUN/$o.log"; }
done
