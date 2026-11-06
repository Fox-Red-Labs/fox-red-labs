#!/bin/bash
# Mac check, no hardware: build the scalar and SSE references, decode both clips, compare scalar against SSE and against ffmpeg's decoder.
# Expected: scalar and SSE differ by at most 1 in some samples; ffmpeg (after dropping its trimmed start) differs by at most 1.
set -e; cd "$(dirname "$0")/.."; host/build.sh >/dev/null; R=results/host
for c in stereo128 mono64; do
  ch=2; [ $c = mono64 ] && ch=1
  $R/ref_scalar clips/$c.mp3 $R/$c-scalar.raw | head -1; $R/ref_sse clips/$c.mp3 $R/$c-sse.raw | head -1
  echo -n "$c scalar vs sse:   "; python3 -I tools/pcmdiff.py $R/$c-scalar.raw $R/$c-sse.raw
  ffmpeg -v error -y -i clips/$c.mp3 -f s16le -acodec pcm_s16le $R/$c-ffmpeg.raw
  echo -n "$c ffmpeg vs scalar (drop 2257 samples/ch): "; python3 -I tools/pcmdiff.py $R/$c-ffmpeg.raw $R/$c-scalar.raw --skip $((2257*ch))
done
