#!/bin/bash
# Build and run the Halloween host preview: writes halloween-preview.wav and halloween-ring.csv into the folder given
# (default host/out), runs the listener's clip test, then renders halloween-preview.mp4 (ring animation with the sound) if ffmpeg is installed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/.."
OUT="${1:-$HERE/out}"; mkdir -p "$OUT"
python3 "$R/tools/tracks2c.py" "$OUT/tracks_gen.c" "$R/tracks"   # the tracks in tracks/
cc -O2 -Wall -I"$R/core" -I"$R/songs" -I"$R/halloween" -I"$R/algos" -o "$OUT/halloween_preview" "$HERE/halloween_preview.c" \
   "$R/core/ba_chip.c" "$R/core/ba_dac8311.c" "$R"/halloween/ba_*.c "$R/algos/ba_yin.c" "$OUT/tracks_gen.c"
cc -O2 -Wall -I"$R/core" -I"$R/halloween" -I"$R/algos" -o "$OUT/test_listen" "$HERE/test_listen.c" "$R/halloween/ba_listen.c" "$R/halloween/ba_sfx.c" "$R/algos/ba_yin.c"
"$OUT/test_listen" | tail -9
"$OUT/halloween_preview" "$OUT"
if command -v ffmpeg >/dev/null; then python3 "$HERE/ring_video.py" "$OUT"; fi
