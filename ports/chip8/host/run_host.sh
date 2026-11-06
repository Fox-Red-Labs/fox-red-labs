#!/bin/bash
# Host check of the CHIP-8 core against the chip8-test-suite ROMs (fetched, not stored). No board needed.
#   bash ports/chip8/host/run_host.sh            compares each final screen's SHA-256 with host/expected.sha256
#   bash ports/chip8/host/run_host.sh --update   rewrites host/expected.sha256 (after looking at the screens: tools/txt2png.py)
# The expected hashes were taken from screens checked by eye against the test suite's own pictures: every check mark on the Corax+ and
# flags screens, all six quirk lines "ok" for the CHIP-8 platform, both logos, the keypad test with keys 1 and 5 held.
set -e
cd "$(dirname "$0")/.."
R="${ROMS:-$PWD/out/chip8/roms}"; [ -f "$R/3-corax+.ch8" ] || tools/fetch_tests.sh "$R" >/dev/null
T=$(mktemp -d); cc -O1 -Wall -Wextra -o "$T/c8host" host/test_host.c src/chip8.c
run() { echo "$1 $("$T/c8host" "$R/$2.ch8" $3 $4 12 "$5" | shasum -a 256 | cut -d' ' -f1)"; }
{ run logo 1-chip8-logo 0 120 ""; run ibm 2-ibm-logo 0 120 ""; run corax "3-corax+" 0 300 ""; run flags 4-flags 0 600 ""
  run quirks 5-quirks 1 700 ""; run keypad 6-keypad 0 260 "150:1:1,160:1:0,200:5:1,230:5:1"; } > "$T/got.txt"
if [ "$1" = "--update" ]; then cp "$T/got.txt" host/expected.sha256; echo updated; else
  if diff "$T/got.txt" host/expected.sha256 >/dev/null; then echo "CHIP-8 host: 6 of 6 screens match"; else diff "$T/got.txt" host/expected.sha256; rm -rf "$T"; exit 1; fi; fi
rm -rf "$T"
