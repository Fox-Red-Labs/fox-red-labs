#!/bin/bash
# Flash the .out over the M3010001 probe (always under the bench lock). Never touches NONMAIN.
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
CCXML=$HOME/tmp/fox-red-labs/mspm33c321a/MSPM33C321A_M3010001.ccxml
python3 "$(dirname "$0")/../../../../tools/frl/lock.py" exec --serial "${PROBE:?set PROBE to the probe serial}" -- "$DSLITE" flash -c "$CCXML" -e -f -v "$(dirname "$0")/ticlang/usart_dma_rx_idle_tx.out"
