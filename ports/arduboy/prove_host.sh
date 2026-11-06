#!/bin/bash
# One command: bash prove_host.sh
# Builds all three games for the host and runs each with scripted input. Writes PNG frames to
# out/frames and prints the checks. Exit code 0 only if every check passes.
set -e
cd "$(dirname "$0")"
mkdir -p out/frames
bash build_host.sh snake >/dev/null
bash build_host.sh tileworld >/dev/null
bash build_host.sh crate-confusion >/dev/null
rc=0
run() { name=$1; shift; bin=${name%%-1bpp*}; out/host/ab_$bin --name $name --png-dir out/frames --scale 2 "$@" || rc=1; }
run snake --frames 240 --script scripts/snake.txt --dump 5,25,60,130,200 --expect-differ 25:60 --expect-differ 60:130 --expect-differ 130:200 --expect-tone
run crate-confusion --frames 320 --script scripts/crate-confusion.txt --dump 10,25,40,70,110,150 --expect-differ 10:25 --expect-differ 25:40 --expect-differ 70:110 --expect-differ 110:150 --expect-tone
run tileworld --frames 140 --script scripts/tileworld.txt --dump 10,70,110 --expect-lit 10 --expect-differ 10:70 --expect-differ 70:110
# Sharp-style 1-bit panel with the 2x vertical fit, same game and script
run snake-1bpp-2xv --frames 240 --script scripts/snake.txt --dump 130 --expect-lit 130 --fmt 1bpp --fit 2xv 2>&1 | sed 's/^snake-1bpp-2xv/snake (1bpp, 2x vertical)/'
exit $rc
