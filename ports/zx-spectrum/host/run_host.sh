#!/bin/bash
# Host checks of the Spectrum machine (ROM-less test image, no Sinclair code). No board needed.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d); python3 tools/mkrom.py "$T/rom.h" "$T/rom.bin" >/dev/null
cc -O2 -Wall -Wextra -Wno-unused-parameter -I../../third-party/chips -o "$T/zxhost" host/test_host.c src/zx.c
"$T/zxhost" "$T/rom.bin" 120; rc=$?; rm -rf "$T"; exit $rc
