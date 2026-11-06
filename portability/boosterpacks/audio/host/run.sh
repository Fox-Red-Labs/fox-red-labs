#!/bin/bash
# Build and run the host test; writes out/tone_1khz.wav and out/fox_run.wav
set -e
cd "$(dirname "$0")"; mkdir -p out
cc -O2 -Wall -I../core -I../songs -o out/test_host test_host.c ../core/*.c -lm
./out/test_host out
