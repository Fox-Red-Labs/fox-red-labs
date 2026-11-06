#!/bin/bash
# Build the patched engine for the Mac with a headless platform layer (frames to PPM, demo playback, zone high-water report).
#   host/build.sh <label> [extra flags]      -> build/host-<label>/doom_host     e.g. host/build.sh s3 "$SQ_S3" after `. srclist.sh`
# Needs ./fetch.sh run first.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; . "$HERE/srclist.sh"
L=${1:-s0}; shift || true; X="$*"
SRC="$HERE/src-upstream/doomgeneric"; O="$HERE/build/host-$L"
[ -d "$SRC" ] || { echo "run ports/doom/fetch.sh first"; exit 2; }
rm -rf "$O"; mkdir -p "$O"
for f in $SRCS w_file_stdc; do cc -Os -w -DFOX_DIRECT -DFOX_ZSTATS -DFOX_HOST -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE $X -I"$SRC" -c "$SRC/$f.c" -o "$O/$f.o" & done; wait
cc -Os -w -DFOX_DIRECT -DDG_HEADLESS_MAIN -I"$SRC" -c "$HERE/host/dg_headless.c" -o "$O/dg_headless.o"
cc "$O"/*.o -lm -o "$O/doom_host"
echo "built $O/doom_host"
