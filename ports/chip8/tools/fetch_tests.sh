#!/bin/bash
# Fetch the chip8-test-suite ROMs (GPL-3.0, so they are fetched, never committed) into $1 (default out/chip8/roms).
set -e
OUT="${1:-$PWD/out/chip8/roms}"; REV=742e9eac9f5d44466c1c9d83fcaa8158c6683cb1
T=$(mktemp -d); mkdir -p "$OUT"
git clone -q https://github.com/Timendus/chip8-test-suite "$T/s"; git -C "$T/s" checkout -q $REV
cp "$T"/s/bin/[1-6]-*.ch8 "$OUT"/; cp "$T/s/LICENSE" "$OUT/LICENSE-chip8-test-suite"; rm -rf "$T"
ls -l "$OUT"
