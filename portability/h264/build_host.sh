#!/bin/bash
# Mac reference: same sources, same harness, cc -O3. Prints one H264R line per stream and pass.   build_host.sh [streams...]   (default: all in streams/)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; W="${W:-$PWD/out/h264/work}"
H=$("$HERE/fetch_src.sh" "$W"); D="$W/host"; rm -rf "$D"; mkdir -p "$D"
S="$@"; [ -n "$S" ] || S="$(ls "$HERE"/streams/*.h264)"
"$HERE/gen_streams.sh" "$D/streams.c" $S
for f in "$H"/*.c; do cc -O3 -std=gnu99 -w -Dmalloc=h_malloc -Dfree=h_free -include stdlib.h -I"$H" -I"$HERE/shim" -c "$f" -o "$D/$(basename "$f" .c).o"; done
cc -O3 -std=gnu99 -I"$H" -I"$HERE" -DPASSES=${PASSES:-2} -c "$HERE/h264_run.c" -o "$D/h264_run.o"
cc -O3 -DARENA_BYTES=${ARENA:-16000000} -c "$HERE/h_alloc.c" -o "$D/h_alloc.o"
cc -O3 -I"$HERE" -c "$HERE/host_main.c" -o "$D/host_main.o"; cc -O3 -I"$HERE" -c "$D/streams.c" -o "$D/streams.o"
cc "$D"/*.o -o "$D/h264host"; "$D/h264host"
