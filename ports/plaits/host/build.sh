#!/bin/bash
# Mac reference build of the Plaits bench. Strict float: -ffp-contract=off, no fast-math, same as the board build.
#   host/build.sh [out-dir]   -> <out-dir>/ref (default $PWD/out/synth/host); then run: ref v > ref.txt
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; TP="$REPO/third-party/plaits"
OUT="${1:-$PWD/out/synth/host}"; mkdir -p "$OUT"
SRCS=$(find "$TP" -name '*.cc' | sort)
clang++ -std=c++11 -O2 -ffp-contract=off -DTEST -include stdio.h -I"$TP" -I"$PORT/common" -w $SRCS "$PORT/common/plaitsrun.cc" "$HERE/main.cc" -o "$OUT/ref"
echo "built $OUT/ref"
