#!/bin/bash
# Flash a .out to the LP-MSPM0G3507 on probe MG350001 with UniFlash, then run.
# Default erase method for MSPM0 in UniFlash 9.6 is "Erase MAIN memory only" (NONMAIN untouched).
set -e
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e 2>/dev/null | grep -q "Serial Num: *MG350001" || { echo "Probe MG350001 missing"; exit 3; }
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c "$(dirname "$0")/MSPM0G3507_MG350001.ccxml" -e -f -v -u "$1"
