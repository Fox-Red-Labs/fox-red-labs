#!/bin/bash
# The live, hands-free microphone run: the Mac speaks near the board, the board listens through its own microphone.
# Needs the CC1407P wires (orange PB10, purple PB11) off J20, and jumper J6 on. Take the bench lock first:
#   mkdir "${TMPDIR:-/tmp}/mspm0g5187.lock"   (rmdir it when done)
#   bash mspm0g5187/wakeword/run_live.sh [MIC_GAIN]      default gain 1; raise it if the sanity line says the level is low
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PY=$HOME/tinyml-bench/venv/bin/python; G=${1:-1}; D=$(date +%Y%m%d-%H%M)
cd "$HERE"; mkdir -p results
MIC_GAIN=$G MODE=i2s bash build.sh | tail -2
bash ../bench/dsflash.sh out/wakeword_i2s_O2.out
osascript -e 'set volume output volume 70'
echo "== sanity: 8 s of the Mac speaking, the board's level lines"
( say -v Samantha "OK Kilby. OK Kilby. Testing the microphone one two three." & ) ; $PY ser.py /dev/cu.usbmodemMG0200011 921600 8 | tee results/live-sanity-$D.txt | cut -c1-200
grep -q "peak=[1-9]" results/live-sanity-$D.txt || { echo "MICROPHONE SILENT (peak 0 or raw words constant): check J20 wires, jumper J6"; exit 2; }
$PY live.py --mode wake --voices Samantha Daniel --n 20 --vol 70 --out results/live-voices-gain$G-$D.txt
$PY live.py --mode silence --secs 600 --out results/live-silence-gain$G-$D.txt
$PY live.py --mode speech --secs 600 --voices Samantha Daniel Karen --vol 70 --out results/live-speech-gain$G-$D.txt
