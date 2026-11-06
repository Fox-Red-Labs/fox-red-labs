#!/bin/bash
# Back up an LP-EM-CC2755P10 with DSLite, twice: main flash 0..0xE8000, CCFG 2 kB, SCFG 1 kB.
#   ports/openocd-cc23xx/gate/backup.sh <out folder>     (SERIAL=<XDS110 serial>)
# DSLite's memory length is in 32-bit words. Writes nothing to the board. Keep the folder outside the repo.
set -u
R="${1:?usage: backup.sh <out folder>}"; mkdir -p "$R"; R=$(cd "$R" && pwd)
REPO=$(cd "$(dirname "$0")/../../.." && pwd); D=/Applications/ti/uniflash_9.6.0/dslite.sh
C=$REPO/cc2755p10/CC2755P10.ccxml   # selects its probe by serial; check it matches SERIAL
L="python3 $REPO/tools/frl/lock.py exec --serial ${SERIAL:?set SERIAL to the XDS110 serial} --"
for n in 1 2; do
  for a in "flash 0x0 0x3A000" "ccfg 0x4E020000 0x200" "scfg 0x4E040000 0x100"; do
    set -- $a
    t0=$(python3 -c 'import time;print(time.time())')
    $L $D --mode memory -c $C -r $2,$3 -o $R/backup-$1-$n.bin > $R/backup-$1-$n.log 2>&1; rc=$?
    t1=$(python3 -c 'import time;print(time.time())')
    printf "%s run %s rc=%s bytes=%s secs=%.1f sha256=%s\n" $1 $n $rc $(stat -f %z $R/backup-$1-$n.bin 2>/dev/null || echo 0) $(python3 -c "print($t1-$t0)") $(shasum -a 256 $R/backup-$1-$n.bin 2>/dev/null | cut -c1-64)
  done
done | tee $R/backup-summary.txt
for f in flash ccfg scfg; do cmp -s $R/backup-$f-1.bin $R/backup-$f-2.bin && echo "$f: two reads identical" || echo "$f: READS DIFFER"; done | tee -a $R/backup-summary.txt
