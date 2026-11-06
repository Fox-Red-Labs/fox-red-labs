#!/bin/bash
# Build the UART0 loop image (checkpoint W3) into out/cc35x1/uartloop/
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/cc35x1/uartloop"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/u"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/uart2echo" "$C"
cp "$HERE/uart_loop.c" "$C/uart2echo.c"; sed -i '' 's/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE 4096/' "$C/freertos/main_freertos.c"
cat >> "$C/freertos/uart2echo.syscfg" <<'SYS'

/* second UART on the header: UART0 TX GPIO17 (pin 4), RX GPIO18 (pin 3) */
var uart3 = UART2.addInstance();
uart3.$name = "CONFIG_UART2_1";
SYS
make -C "$C/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
 && echo "ok: $C/freertos/ticlang/uart2echo.out" || { echo FAILED; grep -m4 -i "error\|warning" "$RUN/b.log"; }
