#!/bin/bash
# Build and run the dataset-stream host test on the Mac (no board). Prints OK on success.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; T="${TMPDIR:-/tmp}/ds_host_test.$$"
cc -std=c99 -Wall -Wextra -Werror -I"$HERE/../core" -o "$T" "$HERE/host_test.c" "$HERE/../core/ds_stream.c" "$HERE/../core/bmi160.c"
"$T"; rc=$?; rm -f "$T"; exit $rc
