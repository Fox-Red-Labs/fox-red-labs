#!/bin/bash
# One command: bash speed/run_steps.sh [frames [skip]]
# Builds the null-display Peanut-GB image for the LP-MSPM33C321A once per speed-up step (cumulative) and counts the instructions per
# frame of each on a Cortex-M33 model (speed/emu_pgb.py, needs `pip install unicorn`; set PY to the python that has it).
# Env: SCRIPT=scripts/tobu.txt feeds scripted buttons (use with a window in the game: bash speed/run_steps.sh 100 2300). Output: $OUTD/<step>.txt (OUTD, default out/speed). Steps, each adding to the one before: base (the 2026-10-20 board image: -Os, averaging scaler, sprite sorting),
# acc0 (no sprite sorting), lut (table scaler), o2 (-O2), skip (one frame in two drawn), ram (emulator core in SRAM). o3 is -O3 for comparison.
set -e
cd "$(dirname "$0")/.."
PY=${PY:-python3}; N=${1:-100}; SKIP=${2:-30}   # frames counted, frames run first (the intro logos start the count)
OUTD=${OUTD:-out/speed}; mkdir -p $OUTD
step() { local tag=$1 fopt=$2 skippatch=$3; shift 3
  # steps up to o3 are built without patch 0002 (own patched copy), later ones with it
  if [ -n "$skippatch" ]; then export PGB_SKIP_PATCH=0002 PGB_PATCHED=out/pg-no0002; else unset PGB_SKIP_PATCH PGB_PATCHED; fi
  TAG=$tag FOPT=$fopt PGB_FLAGS="$*" PGB_SCRIPT=${SCRIPT:-} bash build_target.sh mspm33c321a rom > $OUTD/$tag.build.txt 2>&1
  $PY -I speed/emu_pgb.py out/target/mspm33c321a-rom-$tag/pgb.out --frames $N --skip $SKIP > $OUTD/$tag.txt 2>&1 &
}
step base -Os 1 -DPGB_SCALER_AVG -DPGB_HIGH_LCD_ACCURACY=1
step acc0 -Os 1 -DPGB_SCALER_AVG
step lut  -Os 1
step o2   -O2 1
step o3   -O3 1
step zero -O2 ""
step skip -O2 "" -DPGB_FRAME_SKIP
step ram  -O2 "" -DPGB_FRAME_SKIP -DPGB_RAMFUNC
wait
for t in base acc0 lut o2 o3 zero skip ram; do echo "== $t: $(grep SIZE $OUTD/$t.build.txt)"; sed -n 2,6p $OUTD/$t.txt; done
