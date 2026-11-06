#!/bin/sh
# Host test for the drawing and font code. Needs only a C compiler named cc.
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/edumkii_test_gfx"
cc -std=c99 -Wall -Wextra -O1 -I"$HERE/include" "$HERE/test/test_gfx.c" "$HERE"/src/*.c -o "$OUT" && "$OUT"
