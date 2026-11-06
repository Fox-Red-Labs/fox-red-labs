#!/bin/sh
# On the board, after `insmod zrpmsg.ko` and `echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl`.
# Usage: sh bench.sh OUTFILE   Runs every mode pair three times and appends each result block to OUTFILE.
C=/sys/kernel/zrpmsg
out=${1:-/root/zr/bench.out}
run() { echo "$1" > $C/ctl || echo "CMD FAILED: $1" >> $out; cat $C/result >> $out; }
for m in "0 0" "1 1" "0 1" "1 0"; do
  for rep in 1 2 3; do
    echo "=== rep $rep zmode/lmode $m" >> $out
    run "mode $m"
    run "rtt 56 2000"
    run "rtt 496 2000"
    run "lz 64 20000"
    run "lz 496 20000"
    run "zl 64 20000"
    run "zl 496 20000"
  done
done
echo "=== done" >> $out
