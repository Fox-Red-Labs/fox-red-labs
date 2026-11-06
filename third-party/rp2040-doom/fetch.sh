#!/bin/bash
# Fetch rp2040-doom (Graham Sanderson) at one pinned commit into upstream/ (git-ignored), unmodified.
# Our changes are the patches in patches/, applied by ports/doom/m33-whx/build.sh to a copy, never to upstream/.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
COMMIT=29a453c980918a03e40fc8b69b024e7a3bdb5dc2
if [ ! -d upstream/.git ]; then
  rm -rf upstream
  git clone --quiet https://github.com/kilograham/rp2040-doom.git upstream
fi
git -C upstream checkout --quiet -f "$COMMIT"
echo "rp2040-doom at $(git -C upstream rev-parse HEAD)"
