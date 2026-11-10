#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# One command for this build: ./build.sh --board <mspm0g3507|mspm0g3519|mspm0l1306> [--flash] [--probe <serial>]
# Builds with ../driverlib/build.sh. With --flash it takes a backup, writes the image and checks the flash against it,
# all through tools/frl/frl.py (which holds the probe lock and picks the probe; with one LaunchPad on USB no --probe is needed).
# The backup stays in frl's per-user backup folder; put it back with: python3 tools/frl/frl.py restore <folder>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$(basename "$HERE")"
BOARD=""
FLASH=""
PROBE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --board) BOARD="${2:-}"; shift 2 ;;
    --flash) FLASH=1; shift ;;
    --probe) PROBE="${2:?--probe needs a serial}"; shift 2 ;;
    *) echo "usage: ./build.sh --board <mspm0g3507|mspm0g3519|mspm0l1306> [--flash] [--probe <serial>]" >&2; exit 2 ;;
  esac
done
case "$BOARD" in
  mspm0g3507|mspm0g3519|mspm0l1306) ;;
  *) echo "--board is required: mspm0g3507, mspm0g3519 or mspm0l1306" >&2; exit 2 ;;
esac
"$HERE/../driverlib/build.sh" "$BUILD" "$BOARD"
[ -n "$FLASH" ] || exit 0
TOP="$(cd "$HERE/../../.." && pwd)"
IMG="$HERE/../build/$BUILD-$BOARD/hobby.out"
cd "$TOP"
P=""
[ -z "$PROBE" ] || P="--probe $PROBE"
python3 tools/preflight_image.py "$BOARD" "$IMG"
python3 tools/frl/frl.py backup --board "$BOARD" $P
python3 tools/frl/frl.py flash --board "$BOARD" --image "$IMG" $P
python3 tools/frl/frl.py verify-image --board "$BOARD" --image "$IMG" $P
