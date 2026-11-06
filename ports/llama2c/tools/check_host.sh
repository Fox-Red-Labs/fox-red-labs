#!/bin/bash
# Host check, no board and no TI tool: the port must print exactly what the unmodified runq.c prints, on the same int8 file.
#   PYTHON=<python3 with numpy> ./tools/check_host.sh        (about 10 seconds)
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; T="$HERE/../../third-party/llama2c"; O="${OUT:-$HERE/out}"; mkdir -p "$O"
M="${MODEL:-$HERE/model-cache}"; [ -f "$M/stories260K.bin" ] || "$HERE/fetch.sh" "$M"
cc -O3 -o "$O/run_ref" "$T/run.c" -lm; cc -O3 -o "$O/runq_ref" "$T/runq.c" -lm
cc -O2 -Wall -I"$HERE/src" -o "$O/host_q" "$HERE/src/llama2q.c" "$HERE/src/host_main.c" -lm
"${PYTHON:-python3}" -I "$HERE/tools/quantize_q80.py" "$M/stories260K.bin" "$O/q4.bin" 4
n=0
for cfg in "-t 1.0 -p 0.9 -s 42" "-t 0.8 -p 0.9 -s 1" "-t 0.8 -p 0.9 -s 7" "-t 0 -s 3" "-t 1.0 -p 0 -s 9"; do
  for pr in "Once upon a time" "One day" "Lily and Tom"; do
    "$O/host_q" "$O/q4.bin" "$M/tok512.bin" $cfg -n 128 -i "$pr" 2>/dev/null > "$O/a.txt"
    "$O/runq_ref" "$O/q4.bin" -z "$M/tok512.bin" $cfg -n 128 -i "$pr" 2>/dev/null > "$O/b.txt"
    cmp -s "$O/a.txt" "$O/b.txt" || { echo "DIFFERENT: $cfg / $pr"; exit 1; }; n=$((n+1))
  done
done
echo "port == runq.c on $n of $n runs (5 sampler settings x 3 prompts, 128 steps)"
"$O/run_ref" "$M/stories260K.bin" -z "$M/tok512.bin" -t 0 -s 42 -n 128 -i "Once upon a time" 2>/dev/null > "$O/f.txt"
"$O/host_q" "$O/q4.bin" "$M/tok512.bin" -t 0 -s 42 -n 128 -i "Once upon a time" 2>/dev/null > "$O/i.txt"
cmp -s "$O/f.txt" "$O/i.txt" && echo "greedy: int8 text == float32 text (128 steps)" || echo "greedy: int8 text differs from float32"
