#!/bin/bash
# Host proof for the wire packer. Builds TI's unmodified driver + the fake radio on this host to capture the first
# transaction, generates the reference vectors from TI's structs (-fshort-enums), builds the packer test, runs it.
#   build_host.sh <outdir>
# Writes <outdir>/vectors.h, <outdir>/stream.bin (every packed octet), prints "cases N fail 0 ...".
set -e
R=$(cd "$(dirname "$0")/../../.." && pwd); T=$R/third-party/ti-cc140xp-host; P=$R/portability/cc1407p-host; W=$P/wire
OUT=${1:?outdir}; mkdir -p "$OUT"; cd "$OUT"
INC="-I$T/host -I$T -I$P -I$P/shim -I$W"
DRV="$T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c $T/rcl_settings_wisun.c $P/hal_bare.c"
CF="-std=gnu99 -fshort-enums -w $INC"
# 1. the real driver, fake radio: first transaction bytes
cc $CF -DHOST_BUILD $DRV $P/c28x/hooks_fake.c $P/c28x/bench_main.c -o bench_host
./bench_host | sed -n 's/^log //p' > driver_first_txn.txt
# 2. reference vectors from TI's structs
cc $CF -DHOST_BUILD "$W/ref_gen.c" $T/rcl_settings_wisun.c $P/hal_bare.c $P/c28x/hooks_fake.c -Dbench_done=ref_noop -o ref_gen
./ref_gen driver_first_txn.txt > vectors.h
# 3. the packer test
cc -std=c99 -Wall -Wextra -DHOST_BUILD -fshort-enums -I"$OUT" $INC "$W/test_wire.c" "$W/trx_wire.c" $T/rcl_settings_wisun.c -o test_wire
./test_wire
shasum -a 256 stream.bin | cut -c1-64
