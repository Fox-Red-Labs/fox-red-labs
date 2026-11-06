#!/bin/bash
# Compile check (no link, no board): TI's portable CC1407P/CC1402P TRX host driver for the C28x.
# Source: TI's Zephyr fork simplelink-zephyr v3.7.0-ti-1.00.01_ga, drivers/misc/ti_cc140xp/host (not copied here).
#   check.sh [plain|c2000ware|c2000ware-nopack]   plain: the compiler's own stdint.h; c2000ware: uint8_t as C2000Ware
#   defines it; c2000ware-nopack: the same, plus -D__packed__= so the packed attribute cl2000 rejects becomes empty
# The Zephyr HAL files (hal/platform_spi.c, hal/platform_util.c) are left out: they are the per-host part.
set -u
HOST=${HOST:-$HOME/zephyr-ti/ws/zephyr/drivers/misc/ti_cc140xp/host}
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
HERE="$(cd "$(dirname "$0")" && pwd)"; MODE=${1:-plain}
OUT=$(mktemp -d); LOG="$HERE/logs/c28x-$MODE.txt"
PRE=""; case $MODE in c2000ware) PRE="--preinclude=$HERE/stubs/c2000ware_u8.h";; c2000ware-nopack) PRE="--preinclude=$HERE/stubs/c2000ware_u8.h -D__packed__=";; esac
{
echo "# cl2000 $("$CGT/bin/cl2000" -version 2>&1 | head -1 | sed 's/.*Version/Version/')"
echo "# driver: $HOST ($(git -C "$HOST" describe --tags --always 2>/dev/null))"
echo "# mode: $MODE"
for f in trx_host.c handlers/trx_host_handlers.c private/trx_host_private.c transport/spi_transport.c \
         utility/utils.c utility/list/list.c utility/buffer_manager/buffer_manager.c; do
  echo "== $f"
  "$CGT/bin/cl2000" -v28 -ml -mt --float_support=fpu32 --abi=eabi --opt_level=2 --c99 --display_error_number \
    --issue_remarks $PRE -I"$HOST" -I"$HERE/stubs" -I"$CGT/include" -c "$HOST/$f" --obj_directory="$OUT" 2>&1
  echo "exit $?"
done
} > "$LOG" 2>&1
rm -rf "$OUT"
grep -c '^exit 0' "$LOG" | sed "s/^/files compiled: /"; grep -cE 'error #' "$LOG" | sed 's/^/errors: /'; grep -cE 'warning #' "$LOG" | sed 's/^/warnings: /'
