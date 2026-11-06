#!/bin/bash
# Download the TinyStories 260K model and its 512-token tokenizer (MIT, karpathy/tinyllamas) into a git-ignored folder.
# Every file is checked on every call; a download lands in a temp file and is kept only when its SHA-256 matches.
#   ./fetch.sh [folder]      default folder: ports/llama2c/model-cache
set -euo pipefail
D="${1:-$(cd "$(dirname "$0")" && pwd)/model-cache}"; mkdir -p "$D"
B=https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K
get() {  # name sha256
  local f="$D/$1"
  if [ -f "$f" ] && [ "$(shasum -a 256 "$f" | cut -d' ' -f1)" = "$2" ]; then return 0; fi
  rm -f "$f"; local t; t="$(mktemp "$D/.dl.XXXXXX")"
  curl -fsSL -o "$t" "$B/$1"
  if [ "$(shasum -a 256 "$t" | cut -d' ' -f1)" != "$2" ]; then rm -f "$t"; echo "fetch.sh: $1 failed its SHA-256 check" >&2; exit 1; fi
  mv "$t" "$f"
}
get stories260K.bin b0a507e7ad0f626624f17112325e66691f9076d622e1d3274d103d00299f2696
get tok512.bin 037cb335abb25d1fa9e8ecae30ed2a3a8ace9302862ebcdc05d51a6bbb10c312
