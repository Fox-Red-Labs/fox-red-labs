#!/bin/bash
# One command: bash prove_host.sh
# Needs roms/ (bash fetch_roms.sh). Builds, then: dmg-acid2 renders, Tobu Tobu Girl boots and answers a scripted Start and moves with the
# stick, the same game on a 1-bit panel path, and the frame-rate benchmark. PNG frames go to out/frames. Exit 0 only if every check passes.
set -e
cd "$(dirname "$0")"
bash fetch_roms.sh >/dev/null
bash build_host.sh >/dev/null
mkdir -p out/frames
B=out/host/pgb_host
rc=0
$B --rom roms/dmg-acid2.gb --frames 120 --name acid2 --png-dir out/frames --scale 2 --dump 120 --expect-lit 120 || rc=1
$B --rom roms/tobu.gb --frames 3600 --script scripts/tobu.txt --name tobu --png-dir out/frames --scale 2 \
   --dump 1990,2250,2550,2900,3150,3500 --expect-lit 1990 --expect-differ 1990:2250 --expect-differ 2250:2550 --expect-differ 2550:2900 --expect-differ 2900:3150 || rc=1
$B --rom roms/tobu.gb --frames 2600 --script scripts/tobu.txt --name tobu-1bpp --png-dir out/frames --scale 2 --fmt 1bpp --dump 2550 --expect-lit 2550 || rc=1
# the streaming path a board driver uses (one window, 128 rows, panel byte order) must give the same picture as the line path
h1=$($B --rom roms/tobu.gb --frames 3200 --script scripts/tobu.txt --name s1 --png-dir out/frames --dump 3150 | awk '/^frame 3150/{print $4}')
h2=$($B --rom roms/tobu.gb --frames 3200 --script scripts/tobu.txt --name s2 --png-dir out/frames --dump 3150 --stream | awk '/^frame 3150/{print $4}')
[ -n "$h1" ] && [ "$h1" = "$h2" ] && echo "OK stream picture equals line picture ($h1)" || { echo "FAIL stream picture differs ($h1 / $h2)"; rc=1; }
$B --rom roms/tobu.gb --script scripts/tobu.txt --bench 3600
exit $rc
