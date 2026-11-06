#!/bin/bash
# Run the bootloader check against the LP-MSPM0G3519 (probe MG000001) over its console. Usage: run_g3519.sh [out.json]
H=$(cd "$(dirname "$0")" && pwd)
exec ~/zephyrproject/.venv/bin/python "$H/check.py" /dev/cu.usbmodemMG0000011 \
  --erase "python3 $H/g3519.py erase" --dump "python3 $H/g3519.py dump" --reset "python3 $H/g3519.py reset" ${1:+--json "$1"}
