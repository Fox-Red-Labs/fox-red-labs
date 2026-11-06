#!/bin/bash
# Build and run the algorithm host test (tuner, spectrum, DTMF, gate, echo); writes out/algos_*.wav
set -e
cd "$(dirname "$0")"; mkdir -p out
cc -O2 -Wall -I../core -I../algos -o out/test_algos test_algos.c ../algos/*.c ../core/ba_dac8311.c -lm
./out/test_algos "${1:-../results/am13e230-20261020/console-run3.log}"
