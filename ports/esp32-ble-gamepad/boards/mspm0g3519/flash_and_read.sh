#!/bin/bash
# runs under the probe lock: flash, then read the console for $1 seconds
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c MSPM0G3519_MG000001.ccxml -e -f -v ticlang/frl_gp1.out 2>&1 | tail -3
python3 readcon.py "${1:-9}"
