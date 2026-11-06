#!/bin/sh
# Three ways, Linux: clean build of common/app.c + glue_linux.c on the AM62L EVM itself, inside a Debian
# trixie arm64 container (the board image has no compiler or package manager). Run on the board:
#   sh build.sh /path/to/three-ways          (needs docker and network for the first apt step)
# Output: <three-ways>/linux/build/three_ways (dynamic) and three_ways_static; times and sizes on stdout.
set -eu
TW="$(cd "${1:?three-ways folder}" && pwd)"
docker run --rm --network host -v "$TW":/tw debian:trixie-slim sh -c '
  set -e
  t=$(date +%s); apt-get -qq update >/dev/null && apt-get -qq install -y gcc libc6-dev >/dev/null
  echo "== apt gcc install $(( $(date +%s) - t )) s"
  gcc --version | head -1
  O=/tw/linux/build; rm -rf $O; mkdir -p $O
  CF="-std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -I/tw/common -I/tw/linux"
  t0=$(date +%s%N)
  gcc $CF /tw/common/app.c /tw/linux/glue_linux.c -o $O/three_ways
  t1=$(date +%s%N)
  gcc $CF -static /tw/common/app.c /tw/linux/glue_linux.c -o $O/three_ways_static
  t2=$(date +%s%N)
  echo "== clean build dynamic $(( (t1-t0)/1000000 )) ms, static $(( (t2-t1)/1000000 )) ms"
  cp $O/three_ways $O/three_ways.unstripped; strip $O/three_ways $O/three_ways_static
  ls -l $O; size $O/three_ways.unstripped 2>/dev/null || true
'
