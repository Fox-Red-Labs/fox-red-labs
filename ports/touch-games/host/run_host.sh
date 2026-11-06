#!/bin/bash
# Build and run the host tests (no board needed). Writes proof/2048.png and proof/mines.png from the same game code, drawn into a memory frame buffer.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d); cc -O1 -Wall -Wextra -Wno-unused-parameter -o $T/test_host host/test_host.c host/hal_host.c src/gesture.c src/gfx.c src/font5x7.c src/game2048.c src/mines.c src/ui.c
"$T/test_host"; rc=$?; rm -rf "$T"; exit $rc
