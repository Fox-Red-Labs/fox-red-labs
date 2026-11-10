#!/bin/bash
# CoreMark on R5F core 0 of the LP-AM243: four optimization levels, on-chip RAM and TCM, three runs each, from the shared EEMBC files and port.
# coremark_sweep.sh [log folder]. Builds into $OUT (default out/am243x), writes one console log per run. Needs am243x/tools/build_bench.sh's one-time setup.
cd "$(dirname "$0")/../.." || exit 1
LOGS="${1:-am243x/bench/logs}"; mkdir -p "$LOGS"
export OUT="${OUT:-$PWD/out/am243x}"
. am263p/tools/coremark_files.sh
for mem in ocsram tcm; do for opt in o3 o2 os oz; do
  m=$mem; [ "$mem" = ocsram ] && m=ocram
  am243x/tools/build_bench.sh coremark "$opt" "$m" am263p/bench/coremark/main.c -DITERATIONS=30000 > /dev/null || { echo "BUILD FAIL $opt $mem"; continue; }
  for r in 1 2 3; do
    L="$LOGS/ticlang-bare-$opt-$mem-r$r.txt"
    ~/.venvs/csi/bin/python am243x/tools/ram_capture.py "$OUT/coremark-$opt-$m/coremark.out" "$L" --secs 60 --until "COREMARK END" > /dev/null
    echo "$opt $mem r$r $(grep -a -E 'Iterations/Sec' "$L" | awk '{print $NF}') $(grep -a crcfinal "$L" | awk '{print $NF}') $(grep -a 'Total time' "$L" | awk '{print $NF}')"
  done
done; done
echo SWEEP_DONE
