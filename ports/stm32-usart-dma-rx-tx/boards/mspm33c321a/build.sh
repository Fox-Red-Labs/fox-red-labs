#!/bin/bash
# Build: ./build.sh [baud]   (baud is the single build-time define USART_BAUD, default 115200)
set -e
cd "$(dirname "$0")/ticlang"
SDK=/Applications/ti/mspm33_sdk_1_04_00_00
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SC=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -f *.obj *.out *.map
make MSPM33_SDK_INSTALL_DIR=$SDK SYSCONFIG_TOOL=$SC TICLANG_ARMCOMPILER=$CC GCC_ARMCOMPILER= USART_BAUD=${1:-115200}
grep -E '^ +(BCR_CONFIG|BSL_CONFIG) ' usart_dma_rx_idle_tx.map
$CC/bin/tiarmsize usart_dma_rx_idle_tx.out
