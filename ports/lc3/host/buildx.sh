#!/bin/bash
# Mac reference for the extras bench, strict arithmetic: ./buildx.sh [outdir]  writes refx_strict and prints its CRC lines
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; L="$REPO/third-party/liblc3"
OUT="${1:-$PORT/results/host}"; mkdir -p "$OUT"
SRCS=$(ls $L/src/{attdet,bits,bwdet,energy,lc3,ltpf,mdct,plc,sns,spec,tables,tns}.c)
cc ${CFLAGS:--O2} -ffp-contract=off -fno-vectorize -fno-slp-vectorize -I$L/include -I$L/src -I$PORT/common $SRCS "$PORT/common/lc3x.c" "$PORT/host/refx.c" -lm -o "$OUT/refx_strict"
"$OUT/refx_strict" | tee "$OUT/refx_strict.txt"
