#!/bin/bash
# Compile-only check: do the shared post-quantum libraries build with the TI C2000 compiler (C28x, 16-bit char)?
#   build.sh kem|dsa|core [extra cl2000 flags]     writes objects and the log under $OUT (default: the scratch folder given by OUT)
# Never touches a probe or a board.
set -u
REPO="$(cd "$(dirname "$0")/../../../.." && pwd)"
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
OUT="${OUT:-/Volumes/Seagate/frl-runs/2026-11-09-bench-queue/f2800137/pqc}"; mkdir -p "$OUT"; cd "$OUT"
W="${1:?kem|dsa|core}"; shift
CF="--opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --diag_warning=225 --diag_suppress=10063 -I$CGT/include $*"
PJ="$REPO/portability/pqc-join/core"
case "$W" in
 kem)  "$CGT/bin/cl2000" $CF -DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I"$REPO/third-party/mlkem-native/mlkem" -I"$PJ" -c "$REPO/third-party/mlkem-native/mlkem/mlkem_native.c" --output_file=mlkem_native.obj;;
 dsa)  "$CGT/bin/cl2000" $CF -DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API -DMLD_CONFIG_REDUCE_RAM -I"$REPO/third-party/mldsa-native/mldsa" -I"$PJ" -c "$REPO/third-party/mldsa-native/mldsa/mldsa_native.c" --output_file=mldsa_native.obj;;
 core) "$CGT/bin/cl2000" $CF -I"$PJ" -c "$PJ/pqc_sym.c" --output_file=pqc_sym.obj;;
esac
