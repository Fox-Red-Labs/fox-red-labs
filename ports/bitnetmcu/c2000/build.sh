#!/bin/bash
# Build BitNetMCU's 64-wide CNN timing image for the LAUNCHXL-F2800137. Touches no hardware.
#   ./build.sh <RAM|FLASH>      -> build-<mode>-O4s5/bench.out
# --opt_level=4 --opt_for_speed=5 through ../../../c2000-launchpad/bench/build.sh (TI C2000 compiler 25.11.1.LTS,
# C2000Ware 26.02 device.c). Only main.c is compiled as a unit (it includes bitnet_run.c, which includes the
# model and upstream/BitNetMCU_inference.c), so it is built from a work folder holding main.c alone.
set -e
MODE=$1; HERE="$(cd "$(dirname "$0")" && pwd)"; B="$HERE/../../../c2000-launchpad/bench"
[ "$MODE" = RAM ] || [ "$MODE" = FLASH ] || { echo "usage: $0 <RAM|FLASH>"; exit 2; }
W="$HERE/.work-$MODE"; rm -rf "$W"; mkdir -p "$W"; cp "$HERE/main.c" "$W/"
O="$HERE/build-$MODE-O4s5"; rm -rf "$O"; mkdir -p "$O"
# stack: BitMnistInference keeps layer_out (512 words) and layer_in (1,024 words) on it
if ! STACK=${STACK:-0x800} TAG=out ${LNK:+LNK_RAM=$LNK} "$B/build.sh" "$W" 4 $MODE --opt_for_speed=5 --c99 \
     --preinclude="$HERE/c28_int8.h" -I"$HERE" -I"$HERE/upstream" '-DMODEL_H="BitNetMCU_model_cnn_64.h"' > "$O/build.log" 2>&1; then
  cp "$W/build-out/link.log" "$O/" 2>/dev/null || true; rm -rf "$W"; echo "FAILED: see $O/build.log"; tail -15 "$O/build.log"; exit 1; fi
cp "$W/build-out/"{bench.map,bench.out,link.log} "$O/"; rm -rf "$W"
tail -2 "$O/build.log"
