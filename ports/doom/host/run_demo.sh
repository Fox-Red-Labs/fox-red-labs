#!/bin/bash
# Run a built host engine on a demo and report; frames land in build/frames/ as PPM and PNG (git-ignored).
#   host/run_demo.sh <label> [demo1|demo2|demo3] [every N frames, default 35] [frames, 0 = all]
# Prints the engine's own timing line, the zone high-water marks, the render-limit maxima and the frame checksum.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
L=${1:-s0}; D=${2:-demo1}; N=${3:-35}; F=${4:-0}
mkdir -p "$HERE/build/frames"; rm -f "$HERE/build/frames/"*
DOOM_FRAMES_DIR="$HERE/build/frames" DOOM_FRAME_EVERY=$N DOOM_MAX_FRAMES=$F "$HERE/build/host-$L/doom_host" -iwad "$HERE/wad-cache/DOOM1.WAD" -timedemo "$D" -nosound 2>&1 | grep -E "^(timed|ZSTATS|LIMITS|HOSTFRAMES)"
[ "$N" -gt 0 ] && python3 -I "$HERE/host/ppm2png.py" "$HERE/build/frames/"*.ppm
ls "$HERE/build/frames" | wc -l | tr -d ' ' | sed 's/$/ frame files in build\/frames/'
