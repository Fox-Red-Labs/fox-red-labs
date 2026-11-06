#!/bin/bash
# Fetch the pinned engine (doomgeneric, GPL-2.0) and the shareware WAD into git-ignored folders, apply patches/.
#   ./fetch.sh            engine into src-upstream/, patched; WAD into wad-cache/DOOM1.WAD (checked by SHA-1)
#   ./fetch.sh --no-wad   engine only
# The WAD is id Software's shareware episode. Terms and why it is never committed: LICENSING.md.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
COMMIT=e257a09527062bfb986b6e5e0780839b6cbd5b71
WAD_SHA1=5b2e249b9c5133ec987b3ea77596381dc0d6bc1d      # DOOM1.WAD 1.9, 4,196,020 bytes
if [ ! -d src-upstream/.git ]; then
  rm -rf src-upstream
  git clone --quiet https://github.com/ozkl/doomgeneric.git src-upstream
fi
git -C src-upstream checkout --quiet -f "$COMMIT"
git -C src-upstream clean -fdq
for p in patches/*.patch; do patch -s -p1 -d src-upstream < "$p"; done
echo "engine: $(git -C src-upstream rev-parse HEAD) with $(ls patches/*.patch | wc -l | tr -d ' ') patch(es) applied"
if [ "$1" != "--no-wad" ]; then
  mkdir -p wad-cache
  # A third-party mirror of the shareware file; the SHA-1 above is what counts.
  [ -f wad-cache/DOOM1.WAD ] || curl -fsSL https://raw.githubusercontent.com/wasm3/pywasm3-doom-demo/main/doom1.wad -o wad-cache/DOOM1.WAD
  GOT=$(shasum wad-cache/DOOM1.WAD | cut -d' ' -f1)
  if [ "$GOT" != "$WAD_SHA1" ]; then
    echo "WAD SHA-1 is $GOT, expected $WAD_SHA1 (DOOM1.WAD 1.9). Not using it."; rm -f wad-cache/DOOM1.WAD; exit 1
  fi
  echo "wad: wad-cache/DOOM1.WAD ($(wc -c < wad-cache/DOOM1.WAD | tr -d ' ') bytes, SHA-1 ok)"
fi
