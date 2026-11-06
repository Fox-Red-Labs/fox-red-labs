#!/bin/bash
# Control for check.sh: the same seven portable files, same stubs, for the Cortex-M33 with TI Arm Clang 5.1.1.
HOST=${HOST:-$HOME/zephyr-ti/ws/zephyr/drivers/misc/ti_cc140xp/host}; HERE="$(cd "$(dirname "$0")" && pwd)"; T=$(mktemp -d)
ARM=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; LOG="$HERE/logs/arm-m33.txt"
{ for f in trx_host.c handlers/trx_host_handlers.c private/trx_host_private.c transport/spi_transport.c \
         utility/utils.c utility/list/list.c utility/buffer_manager/buffer_manager.c; do
  echo "== $f"; "$ARM/bin/tiarmclang" -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -Wall -I"$HOST" -I"$HERE/stubs" -c "$HOST/$f" -o "$T/x.o" 2>&1; echo "exit $?"
done; } > "$LOG"; rm -rf "$T"
grep -c '^exit 0' "$LOG" | sed 's/^/files compiled: /'; grep -c 'error:' "$LOG" | sed 's/^/errors: /'; grep -c 'warning:' "$LOG" | sed 's/^/warnings: /'
