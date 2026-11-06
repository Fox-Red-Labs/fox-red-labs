#!/bin/bash
# Build the LP-MSPM0G3519 port of MaJerle/stm32-usart-uart-dma-rx-tx (ringbuff_tx example) into a scratch tree, so the repo stays clean.
#   ./build.sh [outdir] [baud]     default outdir out/stm32-usart-dma-rx-tx-g3519, baud 115200; writes <outdir>/usart_dma.out
# Tools are found the way frl finds them (tools/frl/toolenv.py: MSPM0 SDK, SysConfig, TI Arm Clang; FRL_* variables override).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); PORT=$HERE/../..; REPO=$PORT/../..
OUT=${1:-$PWD/out/stm32-usart-dma-rx-tx-g3519}; BAUD=${2:-115200}
T=$OUT/tree; rm -rf "$T"; mkdir -p "$T/boards/mspm0g3519" "$T/common"
cp -R "$HERE/ticlang" "$HERE/usart_dma.c" "$HERE/usart_dma.syscfg" "$T/boards/mspm0g3519/"
ln -s "$PORT/common/lwrb" "$T/common/lwrb"
export FRL_SYSCONFIG=${FRL_SYSCONFIG:-/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh}   # the version the port was built with
ARGS=(); while IFS= read -r -d '' a; do ARGS+=("$a"); done < <(python3 "$REPO/tools/frl/toolenv.py" mspm0)
cd "$T/boards/mspm0g3519/ticlang"
make "${ARGS[@]}" BAUD="$BAUD" > "$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; exit 1; }
cp usart_dma.out "$OUT/usart_dma.out"; echo "built $OUT/usart_dma.out"
