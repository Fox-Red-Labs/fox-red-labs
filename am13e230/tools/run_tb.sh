#!/bin/bash
# Flash a tb image and read its report from RAM, three runs. Logs: bench/logs or data/logs/tb/<name>-run<N>.log
#   run_tb.sh <name> <bench.out>
N=$1; I=$2; HERE="$(cd "$(dirname "$0")/.." && pwd)"; mkdir -p "$HERE/data/logs/tb"
for r in 1 2 3; do
  { echo "# $(date '+%F %T') load: $(uptime | sed 's/.*load averages*: //')"; python3 "$HERE/tools/run_image.py" "$I" ${WAIT:-20} tb_log:6144 --until tb_flag=0x7b0d0001 --text tb_log 2>&1; } > "$HERE/data/logs/tb/$N-run$r.log"
  grep -c '^R,' "$HERE/data/logs/tb/$N-run$r.log" | sed "s/^/$N run $r: result lines /"
done
