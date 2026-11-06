#!/bin/bash
# Usage: bash patches/apply.sh <out-dir>
# Copies third-party/wasm3/source (unmodified) to <out-dir>/source and applies the patches in this folder to the copy.
# The copy in third-party/ is never changed. WASM3_UNPATCHED=1 copies without patching (for the test that shows the patch is needed).
set -e
cd "$(dirname "$0")/.."
O=$1; rm -rf "$O"; mkdir -p "$O"
cp -R ../../third-party/wasm3/source "$O/source"
[ -n "$WASM3_UNPATCHED" ] || for p in patches/*.patch; do (cd "$O/source" && patch -s -p0 < "$OLDPWD/$p"); done
