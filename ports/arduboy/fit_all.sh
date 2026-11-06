#!/bin/bash
# One command: bash fit_all.sh
# Cross-builds every game for every board (and the Peanut-GB front end through ports/peanut-gb) with TI Arm Clang and
# prints one line per build; then rewrites RESULTS-ROWS.csv in both folders (tools/make_rows.py).
set -e
cd "$(dirname "$0")"
rm -rf out/target ../peanut-gb/out/target
for b in mspm0g3507 mspm0g3519 mspm0g5187 mspm33c321a; do
  for g in snake tileworld crate-confusion; do bash build_target.sh $g $b | grep -v '^SIZE'; done
done
( cd ../peanut-gb
  bash fetch_roms.sh >/dev/null
  for b in mspm0g3507 mspm0g3519 mspm0g5187 mspm33c321a; do
    bash build_target.sh $b | grep -v '^SIZE'
    bash build_target.sh $b acid | grep -v '^SIZE'
    bash build_target.sh $b rom | grep -v '^SIZE'
  done )
python3 tools/make_rows.py
