#!/bin/bash
# Run one execute-in-place variant on the LP-AM263P with no boot-switch change:
#   build the variant, split it (tools/xip_split.py), build the flasher with that blob, write and verify the flash
#   from RAM (bench/xip/flasher), then load the RAM part and capture the console. RUNS times (default 3).
#   xip_run.sh <variant dir under bench/xip> [runs]      variants with no flash sections (ocram) skip the flasher
# DONE=<regex> overrides the end-of-run pattern (AI examples: "Golden vectors matched"). Logs: $LOGDIR/<variant>-flasher.txt and <variant>-r<N>.txt (default out/am263p/xip)
set -eu
V="$1"; RUNS="${2:-3}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
LOGDIR="${LOGDIR:-$PWD/out/am263p/xip}"; mkdir -p "$LOGDIR"
PY="${PY:-$HOME/.venvs/csi/bin/python}"
VD="$HERE/bench/xip/$V/am263px-lp/r5fss0-0_freertos"; [ -d "$VD" ] || VD="$HERE/bench/xip/$V/am263px-lp/r5fss0-0_nortos"
FD="$HERE/bench/xip/flasher/am263px-lp/r5fss0-0_nortos"
B=$(bash "$HERE/tools/build_sdk_example.sh" "$VD"); echo "$B"; case "$B" in "BUILD ok"*) ;; *) exit 1;; esac
OUT=$(echo "$B" | awk '{print $4}'); RAM="$(dirname "$OUT")/ram.out"
if python3 "$HERE/tools/xip_split.py" "$OUT" "$HERE/bench/xip/flasher/blob.h" "$RAM" > "$LOGDIR/${V//\//_}-split.txt" 2>&1; then
  cat "$LOGDIR/${V//\//_}-split.txt"
  rm -f "$FD/ti-arm-clang/obj/release/xip_flasher.obj"
  FB=$(bash "$HERE/tools/build_sdk_example.sh" "$FD"); echo "$FB"; case "$FB" in "BUILD ok"*) ;; *) exit 1;; esac
  "$PY" "$HERE/tools/capload.py" "$(echo "$FB" | awk '{print $4}')" "XIPFLASH|ssert" 60 "$LOGDIR/${V//\//_}-flasher.txt" | tail -2
  grep -q "XIPFLASH OK" "$LOGDIR/${V//\//_}-flasher.txt" || { echo "flasher did not report OK"; exit 1; }
else
  echo "no flash sections: RAM-only run"; RAM="$OUT"
fi
for i in $(seq 1 "$RUNS"); do
  L="$LOGDIR/${V//\//_}-r$i.txt"
  "$PY" "$HERE/tools/capload.py" "$RAM" "${DONE:-All tests have passed|[Ff]ail}" 90 "$L" > /dev/null
  if grep -q FRL_AI "$L"; then echo "run $i: $(grep -m1 -o 'FRL_AI.*' "$L") | $(grep -m1 -o 'Golden vectors.*' "$L")"
  else echo "run $i: $(grep -m1 ' 0B ' "$L" | awk '{print $3, $4}') ... $(grep -m1 '4096B' "$L" | awk '{print $3, $4}') $(grep -c 'All tests have passed' "$L") pass"; fi
done
