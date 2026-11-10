#!/bin/bash
# ipc_notify_echo on the LP-AM243: cores 1 to 3 and the M4F first (they wait in IpcNotify_syncAll), then core 0 with the console open.
#   ipc_run.sh <core0 image name: core0_all|core0_nom4|core0_one> <log>
cd "$(dirname "$0")/../.." || exit 1
IMG="$1"; LOG="$2"
I=${I:-$HOME/tmp/_agent/runs/arrivals/am243x/ipc}; RUN=/Applications/ti/ccs2101/ccs/scripting/run.sh
$RUN am243x/tools/mbox_drain.js | grep FRL_MBOX | tr '\n' ' '
AM243X_CORE=Cortex_M4F_0 $RUN am243x/tools/ram_run.js $I/m4.out | grep -E "FRL_RUN" | tr '\n' ' '
for p in "Cortex_R5_0_1 core1" "Cortex_R5_1_0 core2" "Cortex_R5_1_1 core3"; do set -- $p; AM243X_CORE=$1 $RUN am243x/tools/ram_run.js $I/$2.out | grep -E "FRL_RUN" | tr '\n' ' '; done; echo
~/.venvs/csi/bin/python am243x/tools/ram_capture.py $I/$IMG.out "$LOG" --secs 60 --until "FRL_IPC2 END"
