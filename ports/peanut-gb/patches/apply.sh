#!/bin/bash
# Usage: bash patches/apply.sh <out-dir>   (0001: bounds checks, needed for safety; 0002: zero the line buffer a word at a time, speed)
# Copies third-party/peanut-gb/peanut_gb.h and version.all (unmodified) into <out-dir> and applies the patches in this folder
# to the copy. The copy in third-party/ is never changed. Set PGB_UNPATCHED=1 to copy without patching (for the test that
# proves the patch is needed).
set -e
cd "$(dirname "$0")/.."
O=$1; mkdir -p "$O"
cp ../../third-party/peanut-gb/peanut_gb.h ../../third-party/peanut-gb/version.all "$O/"
# PGB_SKIP_PATCH=0002 leaves out the patches whose file name contains that text (to measure a patch by itself)
[ -n "$PGB_UNPATCHED" ] || for p in patches/*.patch; do
  case "$p" in *"${PGB_SKIP_PATCH:-@@none@@}"*) continue ;; esac
  (cd "$O" && patch -s -p0 peanut_gb.h < "$OLDPWD/$p")
done
