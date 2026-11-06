#!/bin/bash
# Mac reference builds of liblc3 v1.1.3 plus the bench: strict (no fast-math, no fused multiply-add) and upstream flags (-ffast-math).
#   ./build.sh [outdir]     writes ref_strict, ref_fast; prints their CRC lines
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; L="$REPO/third-party/liblc3"
OUT="${1:-$PORT/results/host}"; mkdir -p "$OUT"
SRCS="$L/src/attdet.c $L/src/bits.c $L/src/bwdet.c $L/src/energy.c $L/src/lc3.c $L/src/ltpf.c $L/src/mdct.c $L/src/plc.c $L/src/sns.c $L/src/spec.c $L/src/tables.c $L/src/tns.c"
INC="-I$L/include -I$L/src -I$PORT/common"
cc ${CFLAGS:--O2} -ffp-contract=off -fno-vectorize -fno-slp-vectorize $INC $SRCS "$PORT/common/lc3run.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_strict"
cc ${CFLAGS:--O2} -ffast-math $INC $SRCS "$PORT/common/lc3run.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_fast"
for b in ref_strict ref_fast; do echo "== $b"; "$OUT/$b"; done
# Decoded PCM of the 48 kHz configuration, for a sample-by-sample comparison with the board (pcm48k_strict.raw, pcm48k_fast.raw)
cc -O2 -ffp-contract=off -fno-vectorize -fno-slp-vectorize -DLC3_KEEP_OUT $INC $SRCS "$PORT/common/lc3run.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_strict_pcm"
cc -O2 -ffast-math -DLC3_KEEP_OUT $INC $SRCS "$PORT/common/lc3run.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_fast_pcm"
(cd "$OUT" && ./ref_strict_pcm >/dev/null && mv pcm48k.raw pcm48k_strict.raw && ./ref_fast_pcm >/dev/null && mv pcm48k.raw pcm48k_fast.raw && rm ref_strict_pcm ref_fast_pcm)
