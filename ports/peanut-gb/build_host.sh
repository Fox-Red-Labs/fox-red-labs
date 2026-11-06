#!/bin/bash
# One command: bash build_host.sh [out-dir]   (default out/host)
# PGB_FLAGS="-D..." adds front-end switches (see src/pgb_frontend.c): PGB_SCALER_AVG, PGB_HIGH_LCD_ACCURACY=1, PGB_FRAME_SKIP.
# Builds the Peanut-GB front end for the Mac: third-party/peanut-gb/peanut_gb.h (unmodified), src/pgb_frontend.c,
# the shared display/input layer from ports/arduboy, and the headless runner host/pgb_host.cpp.
set -e
cd "$(dirname "$0")"
bash patches/apply.sh out/peanut-gb-patched
O=${1:-out/host}; mkdir -p "$O"
A=../arduboy
cc -std=gnu11 -O2 $PGB_FLAGS -Wall -Wno-unused -I$A/include -Iout/peanut-gb-patched -c src/pgb_frontend.c -o $O/pgb_frontend.o
c++ -std=gnu++17 -O2 -w -fno-exceptions -fno-rtti -I$A/include -c $A/src/frl_core.cpp -o $O/frl_core.o
c++ -std=gnu++17 -O2 -w -I$A/include -Isrc -c host/pgb_host.cpp -o $O/pgb_host.o
c++ $O/pgb_frontend.o $O/frl_core.o $O/pgb_host.o -lz -o $O/pgb_host
echo "built $O/pgb_host"
