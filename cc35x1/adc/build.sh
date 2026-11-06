#!/bin/bash
# ADC conversion rate on the LP-EM-CC35X1: TI's drivers/adcsinglechannel with its first thread replaced by a loop that
# calls ADC_convert (CPU reads, blocking) for 3 s of FreeRTOS ticks and prints the count. Result on the serial port.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/cc35x1/adc"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$RUN"; mkdir -p "$RUN"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/drivers/adcsinglechannel" "$RUN/a"
python3 - "$RUN/a/adcsinglechannel.c" "$HERE/adc_thread.c" <<'PY'
import sys
p,h=sys.argv[1:3]; t=open(p).read(); add=open(h).read()
t=t.replace("void *threadFxn0(void *arg0)","void *threadFxn0_orig(void *arg0)",1)
i=t.index("/*\n *  ======== threadFxn1")
t=t[:i]+add+"\n"+t[i:]
open(p,'w').write(t)
PY
make -C "$RUN/a/freertos/ticlang" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
	&& echo "ok: $RUN/a/freertos/ticlang/adcsinglechannel.out" || { echo FAILED; grep -m3 -i error "$RUN/b.log"; }
