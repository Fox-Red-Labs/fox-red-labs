#!/bin/bash
# Build the CC1407P host (TI driver + our bare HAL + a transmit app) for one core and print flash and RAM.
# usage: build_fit.sh <cpu> <opt> <outdir> [bufs]    cpu: cortex-m0plus | cortex-m33 ; opt: -Os | -O2 ; bufs: BUFFER_MANAGER_NBUFFERS
set -e
CPU=${1:-cortex-m0plus}; OPT=${2:--Os}; OUT=${3:-/tmp/cc1407p-fit}; NB=${4:-}
R=$(cd "$(dirname "$0")/../.." && pwd); T=$R/third-party/ti-cc140xp-host; P=$R/portability/cc1407p-host
C=${TIARMCLANG_BIN:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin}
mkdir -p "$OUT"; cd "$OUT"
# TI hard-codes the buffer pool at 24 units. A smaller pool is built from a copy of the driver with only that number changed.
if [ -n "$NB" ]; then
  rm -rf "$OUT/t"; mkdir -p "$OUT/t"; cp -R "$T/host" "$T/rcl_settings_wisun.c" "$T/rcl_settings_wisun.h" "$OUT/t/"
  sed -i.bak "s/BUFFER_MANAGER_NBUFFERS    (24U)/BUFFER_MANAGER_NBUFFERS    (${NB}U)/" "$OUT/t/host/utility/buffer_manager/buffer_manager.h"
  T="$OUT/t"
fi
DEF=""
DRV="$T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c"
OTHER="$T/rcl_settings_wisun.c $P/hal_bare.c $P/hooks_stub.c $P/app_tx.c"
$C/tiarmclang -mcpu=$CPU -mthumb $OPT -ffunction-sections -fdata-sections -w $DEF -I$T/host -I$T -I$P -I$P/shim \
  $P/link.cmd -Wl,--entry_point=Reset_Handler -Wl,-m,out.map -o out.elf $DRV $OTHER > build.log 2>&1
$C/tiarmsize out.elf
echo "bss largest:"; grep -E "^\s+[0-9a-f]{8}\s+[0-9a-f]{8}\s+\S+ \(\.bss" out.map | sort -k2 -r | head -4
