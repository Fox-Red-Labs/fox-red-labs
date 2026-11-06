#!/bin/bash
# Single-board Thread test on the LP-CC1354P10-1: load TI's OpenThread CLI (FTD), form a new network alone,
# and confirm the board becomes leader. Usage: run_thread_leader.sh <ot-cli-ftd.out>
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
/Applications/ti/ccs2101/ccs/scripting/python/run.sh "$R/cc1354p10/tools/ccs_load_run.py" "$1" | tail -1
sleep 3
python3 "$R/tools/ot_cli.py" /dev/cu.usbmodemLS45013A1 version state "dataset init new" "dataset commit active" \
  "ifconfig up" "thread start" wait:12 state panid channel extpanid networkname rloc16 ipaddr
