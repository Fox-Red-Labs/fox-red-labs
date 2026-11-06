#!/bin/bash
# Camera-style test streams: 30 frames at 30 fps, 600 kbit/s cap, baseline (ref=1, no B) and main (CABAC, ref=2, 2 B-frames), testsrc2 plus a moving box.   mkprofile.sh <outdir>
set -eu
OUT=${1:-.}; mkdir -p "$OUT"
for S in 1920x1080 1280x720 640x360 480x270 320x240; do
 for P in baseline main; do
  if [ $P = baseline ]; then XP="ref=1:bframes=0:keyint=30:scenecut=0"; else XP="ref=2:bframes=2:keyint=30:scenecut=0"; fi
  ffmpeg -y -loglevel error -f lavfi -i "testsrc2=size=${S}:rate=30,drawbox=x='mod(t*180\,iw-48)':y='ih/2+60*sin(t*6)':w=48:h=48:color=red@1:t=fill" \
   -frames:v 30 -c:v libx264 -profile:v $P -b:v 600k -maxrate 600k -bufsize 1200k -x264-params $XP -pix_fmt yuv420p -f h264 "$OUT/prof-$P-$S.h264"
 done
done
ls -l "$OUT"
