#!/bin/bash
# Three ways: every clean build in turn, then the counts. Touches no board. About 3 minutes on this Mac.
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
(cd common && cc -std=c99 -Wall -Wextra -I. test_app.c app.c -o "${TMPDIR:-/tmp}/three_ways_test" && "${TMPDIR:-/tmp}/three_ways_test")
for b in g3507 l1306 g5187; do driverlib/build.sh $b; done
driverlib/build_m33.sh; driverlib/build_am13.sh
tidrivers/build.sh g3507 freertos; tidrivers/build.sh g3507 nortos; tidrivers/build.sh l1306 nortos
tidrivers/build.sh l1306 freertos || echo "(expected: TI's FreeRTOS settings do not fit 4 KB of RAM)"
for b in g3507 l1306; do zephyr/build.sh $b; done
python3 count.py
