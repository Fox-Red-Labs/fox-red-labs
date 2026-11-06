#!/bin/bash
# Host reference run of the voice front end (portability/voice-fe), built with the Mac's clang, fixed point.
#   portability/voice-fe/build_host.sh <out dir>     makes the test signal, builds, runs, writes host.log, aec.raw, out.raw
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"; O="$1"; mkdir -p "$O"; O="$(cd "$O" && pwd)"
. "$HERE/sources.sh"
[ -f "$O/fe_data.c" ] || python3 -I "$HERE/gen_signal.py" "$O" | tee "$O/gen.log"
cc -O2 -std=gnu99 -ffp-contract=off -w ${FE_DEFS:-} $FE_INC -I"$O" $FE_SRCS "$O/fe_data.c" "$HERE/host_main.c" -lm -o "$O/fe_host"
( cd "$O" && ./fe_host "$O" > host.log )
grep -E "^FE,(L|MEM)" "$O/host.log"
