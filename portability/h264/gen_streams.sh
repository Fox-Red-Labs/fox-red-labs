#!/bin/bash
# gen_streams.sh <out.c> <file.h264>...   C arrays (const, so they stay in flash) plus the table h264_streams[] named after the files.
set -eu
OUT=$1; shift
{ echo '#include "h264_run.h"'
  i=0; for f in "$@"; do echo "static const unsigned char s$i[] = {"; xxd -i < "$f"; echo "};"; i=$((i+1)); done
  echo "const struct h264_stream h264_streams[] = {"; i=0
  for f in "$@"; do n=$(basename "$f" .h264); echo "  { \"$n\", s$i, sizeof s$i },"; i=$((i+1)); done
  echo "};"; echo "const unsigned h264_nstreams = $#;"; } > "$OUT"
