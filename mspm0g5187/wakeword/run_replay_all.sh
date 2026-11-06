#!/bin/bash
# Every replay measurement, one after the other (about 45 minutes). Needs the bench lock and out/wakeword_replay_O2.out flashed.
cd "$(dirname "$0")"; PY=$HOME/tinyml-bench/venv/bin/python
$PY replay.py --voices Samantha Daniel Fred Karen Kathy Ralph --n 20 --level -20 --gap 3 --out results/replay-voices-level-20.txt
for L in -40 -30 -10 -3; do $PY replay.py --voices Samantha --n 10 --level $L --gap 3 --out results/replay-level$L.txt; done
$PY replay_long.py --mode speech --secs 600 --level -20 --out results/replay-false-speech-600s.txt
$PY replay_long.py --mode silence --secs 600 --out results/replay-false-silence-600s.txt
