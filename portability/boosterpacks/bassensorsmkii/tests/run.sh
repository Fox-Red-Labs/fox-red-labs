#!/bin/bash
# Host test of the sensor conversions (no hardware): ./run.sh
set -e
H="$(cd "$(dirname "$0")" && pwd)"; O="${TMPDIR:-/tmp}/bs_host_test"
cc -Wall -I"$H/../include" "$H/host_test.c" "$H"/../src/*.c -o "$O" && "$O"
