#!/bin/bash
# Mac reference builds. scalar = no SIMD, no fused multiply-add (what the Cortex-M builds are compared with); sse = minimp3's default on x86.
set -e; cd "$(dirname "$0")"; T=../../../third-party/minimp3; O=${OUT:-../results/host}; mkdir -p "$O"
CF="-O2 -I$T -I../common -Wall"
clang $CF -DMINIMP3_NO_SIMD -ffp-contract=off ref.c dump.c ../common/mp3run.c -o "$O/ref_scalar"
clang $CF ref.c dump.c ../common/mp3run.c -o "$O/ref_sse"
echo "built $O/ref_scalar $O/ref_sse"
