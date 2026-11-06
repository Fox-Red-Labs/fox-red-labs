#!/bin/bash
# System Reset of the AM62L over the on-board XDS110 (J8), no load, no GEL. Usage: am62l-evm/tools/reset.sh
# A warm reset: the ROM re-reads nothing from the switches, TIFS state is not cleared (see AGENTS.md).
R="${RUN:?set RUN to the folder with AM62L_TMDS62LEVM_nogel.ccxml}"
exec /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash --config="$R/AM62L_TMDS62LEVM_nogel.ccxml" --core=0 --reset=3
