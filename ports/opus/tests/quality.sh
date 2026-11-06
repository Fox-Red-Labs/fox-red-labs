#!/bin/bash
# Mac signal-to-noise check for both builds: tests/quality.sh [fixed|float]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; PORT="$HERE/.."; REPO="$(cd "$PORT/../.." && pwd)"; O="$REPO/third-party/opus"; . "$PORT/sources.sh"
K=${1:-fixed}; if [ "$K" = fixed ]; then X="-DOPUS_PORT_FIXED -I$O/silk/fixed"; else X="-DOPUS_PORT_FLOAT -I$O/silk/float"; fi
T=$(mktemp -d); cc -O2 -ffp-contract=off -DHAVE_CONFIG_H -include "$PORT/common/config.h" -DOP_KEEP $X -I"$PORT/common" -I$O/include -I$O/celt -I$O/silk -I$O/src $(srcs $K) "$PORT/common/opusrun.c" "$PORT/tests/quality.c" -lm -o "$T/q"
"$T/q"; rm -rf "$T"
