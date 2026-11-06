#!/bin/bash
# Run the SRAM Zephyr image in Renode (E208). Prints the UART0 capture and exits. Needs renode on PATH
# (brew install renode/tap/renode on a Mac) and the image from setup.sh or rebuild_sram.sh.
set -u
cd "$(dirname "$0")"
ELF="${1:-${ZP:-$HOME/zephyr-am62l}/build-sram-all/zephyr/zephyr.elf}"
LOG=/tmp/am62l-renode-uart.txt; : > "$LOG"
renode --console --disable-xwt -e "set elf @$ELF; set uartlog @$LOG; include @run.resc" 2>&1 | grep -vE "^\s*$" | tail -40
echo "--- UART0 ($LOG):"; cat "$LOG"
