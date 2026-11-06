#!/bin/bash
# Two-board wire check: radio-side server and host-side driver, each its own CCStudio scripting process.
R=/Applications/ti/ccs2101/ccs/scripting/python/run.sh; HERE="$(cd "$(dirname "$0")" && pwd)"
rm -f /tmp/wc_radio_ready /tmp/wc_cmd.txt /tmp/wc_rsp.txt
$R "$HERE/wc_radio_server.py" > /tmp/wc_radio_server.log 2>&1 &
SRV=$!
$R "$HERE/wirecheck_two_boards.py"
wait $SRV; tail -2 /tmp/wc_radio_server.log
