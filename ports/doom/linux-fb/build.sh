#!/bin/sh
# Build the patched engine with the framebuffer layer, on the AM62L EVM inside a Debian trixie arm64 container
# (the board image has no compiler). Run on the board from a copy of ports/doom that holds src-upstream/ (./fetch.sh):
#   sh linux-fb/build.sh /path/to/doom [extra cflags]      -> <doom>/build/fb/doom_fb
set -eu
D="$(cd "${1:?ports/doom folder}" && pwd)"; shift; X="$*"
docker run --rm --network host -v "$D":/d debian:trixie-slim sh -c "
  set -e; apt-get -qq update >/dev/null && apt-get -qq install -y gcc libc6-dev >/dev/null 2>&1
  . /d/srclist.sh; S=/d/src-upstream/doomgeneric; O=/d/build/fb; rm -rf \$O; mkdir -p \$O
  t0=\$(date +%s%N)
  for f in \$SRCS w_file_stdc; do gcc -O2 -w -DFOX_DIRECT -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE $X -I\$S -c \$S/\$f.c -o \$O/\$f.o & done; wait
  gcc -O2 -Wall -DFOX_DIRECT -I\$S -c /d/linux-fb/dg_fbdev.c -o \$O/dg_fbdev.o
  gcc -static \$O/*.o -lm -o \$O/doom_fb
  t1=\$(date +%s%N); echo \"== build \$(( (t1-t0)/1000000 )) ms on 2 cores, gcc \$(gcc -dumpfullversion)\"
  strip -o \$O/doom_fb.stripped \$O/doom_fb; ls -l \$O/doom_fb \$O/doom_fb.stripped
"
