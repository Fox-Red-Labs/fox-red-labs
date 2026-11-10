#!/bin/bash
# CoreMark on all four R5F cores of the LP-AM243 at once. all4.sh build <o3|..> <ocram|tcm>   |   all4.sh run <o3> <ocram|tcm> <log>
# One image per core (am243x/bench/all4/main_all4.c), each in its own 256 kB of OCSRAM; cores 1 to 3 are loaded first, core 0 last
# (its console must be open), then the Mac writes the go word. Needs the one-time setup of am243x/tools/build_bench.sh.
cd "$(dirname "$0")/../.." || exit 1
export OUT="${OUT:-$HOME/tmp/_agent/runs/arrivals/am243x/bench}"
. am263p/tools/coremark_files.sh
RUN=/Applications/ti/ccs2101/ccs/scripting/run.sh
CORES=(Cortex_R5_0_0 Cortex_R5_0_1 Cortex_R5_1_0 Cortex_R5_1_1)
BASES=(0x70080000 0x700C0000 0x70100000 0x70140000)
case "$1" in
build)
  for i in 0 1 2 3; do
    ALL4_MSRAM=${BASES[$i]} LINK_PATCH=am243x/bench/all4/msram_patch.py \
      am243x/tools/build_bench.sh cm4c$i "$2" "$3" am243x/bench/all4/main_all4.c -DITERATIONS=${ITER:-30000} -DALL4_CORE=$i || exit 1
  done;;
run)
  O="$2"; M="$3"; LOG="$4"
  $RUN am243x/bench/all4/go.js 0 | grep FRL_GO
  for i in 1 2 3; do AM243X_CORE=${CORES[$i]} $RUN am243x/tools/ram_run.js "$OUT/cm4c$i-$O-$M/cm4c$i.out" | grep -E "FRL_START|FRL_RUN" | tr '\n' ' '; echo; sleep 2; done
  (sleep 9; $RUN am243x/bench/all4/go.js $((0x600D600D)) | grep FRL_GO) &
  ~/.venvs/csi/bin/python am243x/tools/ram_capture.py "$OUT/cm4c0-$O-$M/cm4c0.out" "$LOG" --secs 90 --until "ALL4 END"
  wait;;
esac
