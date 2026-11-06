#!/bin/bash
# Build and run the fom_sweep() host test on the Mac. Prints OK on success.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; T="${TMPDIR:-/tmp}/fom_host_test.$$"
cc -std=c99 -Wall -Wextra -Werror -o "$T" "$HERE/host_test.c"
"$T"; rc=$?; rm -f "$T"; exit $rc
