#!/bin/sh
# On the board, in /root/frl/lean: the measurement matrix. Three 10 s runs (after 2 s warm-up) per row; one RESULT line each.
# Usage: sh sweep.sh > out/matrix.txt
S=snes9x2005/snes9x2005_libretro.so; P=pcsx_rearmed/pcsx_rearmed_libretro.so; G=./gambatte_libretro.so
run() { # label, then lean_fe args
  l=$1; shift
  for i in 1 2 3; do echo "$l run$i $(./lean_fe -d 10 "$@" 2>&1 | grep RESULT | sed 's/RESULT //')"; done
}
for m in 640x480 800x600 1280x720 1920x1080; do
  run "snes-mode7-$m" -c $S -g roms/Mode7LZ77.sfc -m $m -p
done
run "snes-transp-640x480" -c $S -g roms/Transparency.sfc -m 640x480 -p
run "snes-transp-1920x1080" -c $S -g roms/Transparency.sfc -m 1920x1080 -p
run "snes-mode7-1920x1080-thread" -c $S -g roms/Mode7LZ77.sfc -m 1920x1080 -p -t
run "snes-fxmusic-640x480-audio" -c $S -g roms/effectsandmusic.sfc -m 640x480 -p -a
run "snes-fxmusic-1920x1080-audio-thread" -c $S -g roms/effectsandmusic.sfc -m 1920x1080 -p -a -t
run "snes-transp-640x480-freerun" -c $S -g roms/Transparency.sfc -m 640x480 -p -n
run "snes-transp-1920x1080-freerun" -c $S -g roms/Transparency.sfc -m 1920x1080 -p -n
for m in 640x480 1280x720 1920x1080; do
  run "ps1-bench-$m" -c $P -g roms/benchmark.exe -m $m
done
run "ps1-bench-1920x1080-thread" -c $P -g roms/benchmark.exe -m 1920x1080 -t
run "ps1-bench-640x480-audio" -c $P -g roms/benchmark.exe -m 640x480 -a
run "ps1-bench-640x480-freerun" -c $P -g roms/benchmark.exe -m 640x480 -n
run "gb-tobu-640x480" -c $G -g /root/frl/showcase/roms/tobu.gb -m 640x480 -p
run "gb-tobu-1920x1080" -c $G -g /root/frl/showcase/roms/tobu.gb -m 1920x1080 -p
