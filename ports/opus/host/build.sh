#!/bin/bash
# Mac reference builds of libopus 1.5.2 plus the bench: fixed-point and float, strict arithmetic (no fast-math, no fused multiply-add).
#   ./build.sh [outdir]     writes ref_fixed, ref_float and their CRC lines (ref_fixed.txt, ref_float.txt)
#   Env: INT64=0 also builds ref_fixed32 from a build copy with OPUS_FAST_INT64 forced to 0 (the 32-bit multiply macros the board build uses by default)
#   libopus turns OPUS_FAST_INT64 on for LP64 hosts (celt/arch.h); the board build is ILP32, so the Mac needs the override to compare like with like.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; O="$REPO/third-party/opus"
OUT="${1:-$PORT/results/host}"; mkdir -p "$OUT"
. "$PORT/sources.sh"
INC="-I$PORT/common -I$O/include -I$O/celt -I$O/silk -I$O/src"
FL="-ffp-contract=off -fno-vectorize -fno-slp-vectorize -DHAVE_CONFIG_H -include $PORT/common/config.h"
cc ${CFLAGS:--O2} $FL -DOPUS_PORT_FIXED $INC -I$O/silk/fixed $(srcs fixed) "$PORT/common/opusrun.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_fixed"
cc ${CFLAGS:--O2} $FL -DOPUS_PORT_FLOAT $INC -I$O/silk/float $(srcs float) "$PORT/common/opusrun.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_float"
for b in fixed float; do "$OUT/ref_$b" v | tee "$OUT/ref_$b.txt" | grep -v "^F "; done
if [ "${INT64:-}" = 0 ]; then
  # build copy with OPUS_FAST_INT64 forced to 0 (the original stays untouched)
  C="$OUT/opus-int64-0"; rm -rf "$C"; mkdir -p "$C"; cp -R "$O/include" "$O/celt" "$O/silk" "$O/src" "$C/"
  sed -i.bak 's/^#define OPUS_FAST_INT64 1/#define OPUS_FAST_INT64 0/' "$C/celt/arch.h"
  O="$C"; INC="-I$PORT/common -I$O/include -I$O/celt -I$O/silk -I$O/src"
  cc ${CFLAGS:--O2} $FL -DOPUS_PORT_FIXED $INC -I$O/silk/fixed $(srcs fixed | sed "s#$REPO/third-party/opus#$C#") "$PORT/common/opusrun.c" "$PORT/host/ref.c" -lm -o "$OUT/ref_fixed32"
  "$OUT/ref_fixed32" v | tee "$OUT/ref_fixed32.txt" | grep -v "^F "
fi
