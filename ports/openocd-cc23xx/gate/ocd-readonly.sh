#!/bin/bash
# Read-only OpenOCD session: identity through CFG-AP, AP IDRs, CPUID, then main flash 0..0xE8000 and the
# CCFG through the AHB-AP, compared with backup.sh's DSLite reads in the same folder. No halt, no reset, no write.
#   OPENOCD=<built openocd source tree> ports/openocd-cc23xx/gate/ocd-readonly.sh <backup folder>
set -u
R="${1:?usage: ocd-readonly.sh <backup folder>}"; R=$(cd "$R" && pwd)
O="${OPENOCD:?set OPENOCD to the built openocd tree}"; S=${SERIAL:?set SERIAL to the XDS110 serial}
REPO=$(cd "$(dirname "$0")/../../.." && pwd)
L="python3 $REPO/tools/frl/lock.py exec --serial $S --"
t0=$(python3 -c 'import time;print(time.time())')
$L $O/src/openocd -s $O/tcl -c "adapter serial $S" -f board/ti_lp_em_cc2755p10.cfg \
  -c "gdb port disabled" -c "tcl port disabled" -c "telnet port disabled" -c init \
  -c 'echo "DPIDR [format 0x%08x [cc27xx.dap dpreg 0]]"' \
  -c 'foreach a {0 1 2 3} { echo "AP$a IDR [format 0x%08x [cc27xx.dap apid $a]]" }' \
  -c 'echo "DEVICEID [format 0x%08x [cc27xx.dap apreg 1 0x0]]"' \
  -c 'echo "PARTID [format 0x%08x [cc27xx.dap apreg 1 0x4]]"' \
  -c 'echo "DEBUGSSVERSION [format 0x%08x [cc27xx.dap apreg 1 0x8]]"' \
  -c 'echo "DEVICESTATUS [format 0x%08x [cc27xx.dap apreg 1 0xc]]"' \
  -c 'echo "CPUID [format 0x%08x [read_memory 0xE000ED00 32 1]]"' \
  -c 'echo "state [cc27xx.cpu curstate]"' \
  -c "flash probe 0" -c "flash info 0" \
  -c "dump_image $R/ocd-flash.bin 0x0 0xE8000" \
  -c "dump_image $R/ocd-ccfg.bin 0x4E020000 0x800" \
  -c 'echo "state after [cc27xx.cpu curstate]"' \
  -c shutdown > $R/ocd-readonly.log 2>&1; rc=$?
t1=$(python3 -c 'import time;print(time.time())')
echo "rc=$rc secs=$(python3 -c "print(round($t1-$t0,1))")" | tee -a $R/ocd-readonly.log
for f in flash ccfg; do
  cmp $R/ocd-$f.bin $R/backup-$f-1.bin && echo "$f: OpenOCD dump identical to DSLite backup" || echo "$f: DIFFERS from DSLite backup"
done | tee $R/compare.txt
