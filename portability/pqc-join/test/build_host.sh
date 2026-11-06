#!/bin/bash
# Build the host test programs into $1 (default test/out). Native compiler, same sources the board uses.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../../.." && pwd)"; OUT="${1:-$HERE/out}"; mkdir -p "$OUT"
ML="$ROOT/third-party/mlkem-native/mlkem"; MD="$ROOT/third-party/mldsa-native/mldsa"; CORE="$HERE/../core"
FLAGS="-O2 -std=c99 -Wall -DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API"
cc $FLAGS -I"$ML" -c "$ML/mlkem_native.c" -o "$OUT/mlkem_native.o"
DFLAGS="-O2 -std=c99 -Wall -DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API"
cc $DFLAGS -I"$MD" -c "$MD/mldsa_native.c" -o "$OUT/mldsa_native.o"
cc $DFLAGS -I"$MD" -I"$CORE" -c "$CORE/pqc_sign.c" -o "$OUT/pqc_sign.o"
cc $FLAGS -I"$ML" -I"$CORE" "$HERE/../host/board_host.c" "$CORE/pqc_join.c" "$CORE/pqc_sym.c" "$OUT/mlkem_native.o" "$OUT/pqc_sign.o" "$OUT/mldsa_native.o" -o "$OUT/board_host"
cc -O2 -std=c99 -Wall -I"$CORE" "$HERE/unit_sym.c" "$CORE/pqc_sym.c" -o "$OUT/unit_sym"
