#!/bin/bash
# Make baseline-profile H.264 test streams: 30 frames, 15 fps, one reference frame, no B-frames, testsrc2 plus a moving box.   mkstreams.sh <outdir>
set -eu
OUT=${1:-.}; mkdir -p "$OUT"
for S in 128x96 160x112 176x144 240x240 320x240 540x540; do
  W=${S%x*}; H=${S#*x}
  ffmpeg -y -loglevel error -f lavfi -i "testsrc2=size=${S}:rate=15,drawbox=x='mod(t*90\,iw-24)':y='ih/2+30*sin(t*6)':w=24:h=24:color=red@1:t=fill" \
    -frames:v 30 -c:v libx264 -profile:v baseline -x264-params ref=1:bframes=0:keyint=30:scenecut=0 -pix_fmt yuv420p -f h264 "$OUT/stream-$S.h264"
done
ls -l "$OUT"
