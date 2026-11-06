#!/bin/bash
# E242: run an E242 ring image in Renode and decode the ring. Usage: run-ring.sh [ELF]  (default build-sram-core1-ring). Needs renode on PATH.
set -u
cd "$(dirname "$0")"
ELF="${1:-${ZP:-$HOME/zephyr-am62l}/build-sram-core1-ring/zephyr/zephyr.elf}"
renode --console --disable-xwt -e "set elf @$ELF; include @run-ring.resc" 2>&1 | grep -vE "^\s*$" | tail -30
