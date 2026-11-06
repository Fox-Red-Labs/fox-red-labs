#!/bin/bash
# Host unit test for the Sharp128 core (panel packing, VCOM, drawing, bands). No board needed.
set -e
H="$(cd "$(dirname "$0")" && pwd)"; O="${TMPDIR:-/tmp}/sharp128-host-test"
mkdir -p "$O"
cc -std=c99 -O1 -Wall -Wextra -Wno-unused-function -o "$O/host_test" "$H/host_test.c" "$H/../sharp_lcd.c" "$H/../sharp_gfx.c"
"$O/host_test"
