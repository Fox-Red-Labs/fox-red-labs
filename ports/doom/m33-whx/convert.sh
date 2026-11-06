#!/bin/bash
# Build rp2040-doom's converter (whd_gen) on the Mac and run it on the reader's own DOOM1.WAD.
#   m33-whx/convert.sh [path to DOOM1.WAD, default wad-cache/DOOM1.WAD]     output wad-cache/DOOM1.whx (git-ignored, never committed)
# The converted file is not original data and not ours: see ports/doom/LICENSING.md section 4.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; DOOM="$(cd "$HERE/.." && pwd)"; REPO="$(cd "$DOOM/../.." && pwd)"
TP="$REPO/third-party/rp2040-doom"; [ -d "$TP/upstream/src" ] || "$TP/fetch.sh"
WAD="${1:-$DOOM/wad-cache/DOOM1.WAD}"; [ -f "$WAD" ] || { echo "no WAD at $WAD (run ports/doom/fetch.sh or pass your own DOOM1.WAD)"; exit 2; }
S="$TP/upstream/src"; O="$DOOM/build/whd_gen"; mkdir -p "$O"
cd "$S/whd_gen"
for f in whd_gen mus2seq huff lodepng compress_mus wad; do
  c++ -std=c++17 -O2 -w -Wno-missing-template-arg-list-after-template-kw -DIS_WHD_GEN=1 -I. -I.. -I../doom -I../adpcm-xq -c $f.cpp -o "$O/$f.o" &
done
for f in ../tiny_huff ../musx_decoder ../image_decoder ../adpcm-xq/adpcm-lib; do
  cc -O2 -w -DIS_WHD_GEN=1 -I. -I.. -I../doom -I../adpcm-xq -c $f.c -o "$O/$(basename $f).o" &
done
wait
c++ "$O"/*.o -o "$O/whd_gen"
"$O/whd_gen" "$WAD" "$DOOM/wad-cache/DOOM1.whx" > "$O/whd_gen.log"
tail -22 "$O/whd_gen.log"
ls -l "$DOOM/wad-cache/DOOM1.whx"
