#!/bin/bash
# Run every DSP image N times (default 3), each run a full erase, flash and read under the probe lock.
#   ./runall.sh [N]   -> results/runall.txt (one decoded line per run)
cd "$(dirname "$0")"; N=${1:-3}; : > results/runall.txt
for r in $(seq $N); do
  echo "run $r cbase: $(./run.sh cbase 2>&1 | tail -1)" >> results/runall.txt
  for m in fir64 fir64_dec4 bank8 bank8_trip bank8_trip2 bank8_trip3 bank8_trips dft64; do for t in npu cpu; do
    echo "run $r $(./run.sh $m $t 2>&1 | tail -1)" >> results/runall.txt
  done; done
done
echo done >> results/runall.txt
