#!/bin/bash
# Build the post-quantum KAT and timing image for the LP-AM243 (R5FSS0-0, nortos) with am243x/tools/build_bench.sh. BUILD ONLY.
#   build.sh <o3|o2|os> [kem]     kem = ML-KEM-768 only; default builds ML-KEM-768 and ML-DSA-44 (default parameters, no RAM reduction) in one image
# -> $OUT/pqc<mode>-<opt>-ocram/pqc<mode>.out. Needs the one-time setup of am243x/tools/build_bench.sh ($OUT/gen).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
OPT="${1:?usage: build.sh o3|o2|os [kem]}"; MODE="${2:-both}"
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
export PQC_STACK_BYTES="${PQC_STACK_BYTES:-98304}" LINK_PATCH="$HERE/link_patch.py"
export EXTRA_INC="-I$PJ -I$ML -I$MD"
FL=(-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -DPQC_STACK_BYTES=$PQC_STACK_BYTES -DPQC_OPT="\"-${OPT/o/O}\"" -DPQC_BUILD_ID="\"am243x-pqc-$MODE-$OPT\"")
if [ "$MODE" = kem ]; then
  EXTRA_SRC="$PJ/pqc_join.c $PJ/pqc_sym.c $ML/mlkem_native.c" "$REPO/am243x/tools/build_bench.sh" pqckem "$OPT" ocram "$HERE/pqc_board.c" -DPQC_NO_DSA "${FL[@]}"
else
  EXTRA_SRC="$PJ/pqc_join.c $PJ/pqc_sym.c $PJ/pqc_sign.c $ML/mlkem_native.c $MD/mldsa_native.c" "$REPO/am243x/tools/build_bench.sh" pqcboth "$OPT" ocram "$HERE/pqc_board.c" -DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API "${FL[@]}"
fi
