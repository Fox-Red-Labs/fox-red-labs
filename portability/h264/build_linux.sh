#!/bin/sh
# Native build on a Linux board (gcc), same flags as build_host.sh; timing from CLOCK_MONOTONIC, so cyc= fields are nanoseconds.
# Run on the Mac first to stage a folder: STAGE=<dir> build_linux.sh stage [streams...]; copy <dir> to the board; then on the board: sh build.sh && ./h264lin
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
[ "${1:-}" = stage ] || { echo "usage: STAGE=<dir> $0 stage [streams...]"; exit 1; }; shift
D="${STAGE:?}"; H=$("$HERE/fetch_src.sh"); rm -rf "$D"; mkdir -p "$D/dec"
cp "$H"/*.c "$H"/*.h "$D/dec/"; cp "$HERE"/h264_run.c "$HERE"/h264_run.h "$HERE"/h_alloc.c "$HERE"/linux_main.c "$D/"; cp -R "$HERE/shim" "$D/"
S="$@"; [ -n "$S" ] || S="$(ls "$HERE"/streams/*.h264)"; "$HERE/gen_streams.sh" "$D/streams.c" $S >/dev/null
cat > "$D/build.sh" <<'B'
#!/bin/sh
set -e; cd "$(dirname "$0")"; mkdir -p o
for f in dec/*.c; do cc -O3 -std=gnu99 -w -Dmalloc=h_malloc -Dfree=h_free -include stdlib.h -Idec -Ishim -c $f -o o/$(basename $f .c).o; done
cc -O3 -std=gnu99 -Idec -I. -DPASSES=${PASSES:-3} -c h264_run.c -o o/h264_run.o
cc -O3 -DARENA_BYTES=16000000 -c h_alloc.c -o o/h_alloc.o
cc -O3 -I. -c linux_main.c -o o/linux_main.o; cc -O3 -I. -c streams.c -o o/streams.o
cc o/*.o -o h264lin
B
echo "$D"
