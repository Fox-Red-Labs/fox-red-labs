#!/bin/bash
# Build-only sweep of upstream Zephyr samples for the TI boards, one board at a time. Touches no hardware.
#   ./run.sh                     all boards
#   ./run.sh lp_mspm0g3507 ...   chosen boards (lp_mspm0l1306 uses the out-of-tree board in ../../../mspm0l1306/zephyr)
# Env: ZP (default ~/zephyrproject), TC (default ~/zephyr-toolchain), WORK (build scratch, default /tmp-ish dir
#      under $TMPDIR), JOBS (default 6; each build dir is ~40 MB while it lives, deleted right after summary).
# Output: data/results.jsonl (one row per board and scenario), data/logs/*.gz, data/versions.txt
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
WORK="${WORK:-${TMPDIR:-/tmp}/zephyr-upstream-sweep}"; JOBS="${JOBS:-6}"
BOARDS=("$@"); [ ${#BOARDS[@]} -eq 0 ] && BOARDS=(lp_mspm0g3507 lp_mspm0g3519 lp_mspm0l2228 lp_em_cc2340r5 lp_mspm0l1306)
. "$ZP/.venv/bin/activate"
export ZEPHYR_BASE="$ZP/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
DATA="$HERE/data"; mkdir -p "$DATA/logs" "$WORK"
{ echo "zephyr: $(git -C "$ZP/zephyr" describe --tags)"; echo "toolchain: $("${CROSS_COMPILE}gcc" --version | head -1)"
  echo "west: $(west --version)"; echo "host: $(uname -srm)"; echo "date: $(date -u +%FT%TZ)"
  echo; echo "TI boards known to this Zephyr (west boards, vendor ti):"
  ls "$ZEPHYR_BASE/boards/ti" | grep -v -E '^(common|index.rst)$'
  echo; echo "west modules present in the workspace (trimmed checkout):"; (cd "$ZP" && ls modules/hal modules/lib | sed 's/^/  /')
  echo; echo "python tools: $(pip list 2>/dev/null | grep -i -E '^(west|cmake|natsort|junitparser|pytest|ply) ' | tr -s ' ' | tr '\n' ';')"; } > "$DATA/versions.txt"
# sample groups: each top-level sample dir, plus each drivers/* directory (35 samples sit directly in drivers/)
GROUPS_=(samples/hello_world samples/basic samples/synchronization samples/philosophers)
for d in "$ZEPHYR_BASE"/samples/drivers/*/; do GROUPS_+=("samples/drivers/$(basename "$d")"); done
: > "$DATA/results.tmp.jsonl"
for b in "${BOARDS[@]}"; do
  extra=(); [ "$b" = lp_mspm0l1306 ] && { L13="$(cd "$HERE/../../../mspm0l1306/zephyr" && pwd)"; extra=(--board-root "$L13/boards" -x "BOARD_ROOT=$L13"); }
  for g in "${GROUPS_[@]}"; do
    for try in 1 2 3 4; do
      # wait for 500 MB free: builds die with a disk-full error otherwise, and that is not a result
      while [ "$(df -m "$WORK" | awk 'NR==2{print $4}')" -lt 500 ]; do sleep 20; done
      O="$WORK/$b/${g//\//_}"; rm -rf "$O"
      (cd "$ZP" && python zephyr/scripts/twister -p "$b" ${extra[@]+"${extra[@]}"} --build-only --force-toolchain \
         --report-filtered -M pass -T "zephyr/$g" -O "$O" -j "$JOBS" > "$WORK/last.txt" 2>&1)
      python3 "$HERE/summarize.py" "$O" "$b" "$g" "$DATA/results.tmp.jsonl" "$DATA/logs"; rc=$?
      rm -rf "$O"
      [ $rc -ne 3 ] && break   # 3 = disk-full errors seen, run the group again
    done
  done
done
# merge: replace rows for the boards just run, keep rows of other boards
python3 - "$DATA" "${BOARDS[@]}" <<'P'
import json, sys, os
data, boards = sys.argv[1], set(sys.argv[2:])
keep = []
p = os.path.join(data, "results.jsonl")
if os.path.exists(p):
    keep = [l for l in open(p) if json.loads(l)["board"] not in boards]
new = open(os.path.join(data, "results.tmp.jsonl")).readlines()
open(p, "w").writelines(keep + new); os.remove(os.path.join(data, "results.tmp.jsonl"))
P
python3 "$HERE/report.py" "$DATA/results.jsonl" > "$DATA/summary.json"
"$ZP/.venv/bin/python" "$HERE/analyze.py"
