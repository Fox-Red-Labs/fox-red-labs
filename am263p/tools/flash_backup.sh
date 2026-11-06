#!/bin/bash
# Read the LP-AM263P's whole OSPI flash over the debug probe, twice, and check the two reads match.
#   flash_backup.sh <output dir> [XDS110 serial, default S26E0426]
# Read only: it loads nothing and writes nothing to the board. Uses the no-GEL connection file and DSLite's memory
# read of the memory-mapped window (0x60000000, 8 reads of 4 MB). Our board's reads show the second 16 MB as a copy
# of the first, so the upper half of the 32 MB part is not read by this method.
# To compare a later read with a backup:  cmp -l <old>/full_a.bin <new>/full_a.bin | head
set -eu
O="${1:?output dir}"; S="${2:-${AM263P_SERIAL:-S26E0426}}"; mkdir -p "$O"
HERE="$(cd "$(dirname "$0")/.." && pwd)"; C="$HERE/AM263Px_nogel_$S.ccxml"
D=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
for pass in a b; do rm -f "$O/full_$pass.bin"
  for i in 0 1 2 3 4 5 6 7; do
    A=$(printf "0x%X" $((0x60000000 + i * 4194304)))
    "$D" memory -c "$C" -r "$A,1048576" -o "$O/c.bin" > "$O/c.log" 2>&1 || { echo "FAIL pass $pass read $i"; tail -3 "$O/c.log"; exit 1; }
    cat "$O/c.bin" >> "$O/full_$pass.bin"
  done
done
rm -f "$O/c.bin"
# a read of the window before the boot ROM or a driver has set up the flash controller returns one value throughout
# (all zeros after a debugger System Reset, and after a power cycle with the boot switches in development boot,
# seen 2026-10-11): refuse to call that a backup
python3 -c "import sys; d=open(sys.argv[1],'rb').read(1<<20); sys.exit(0 if len(set(d)) > 1 else 1)" "$O/full_a.bin" || {
  echo "FAIL: the first 1 MB is one byte value throughout; the flash window is not mapped. In development boot nothing maps it at power-up: our good reads all came right after a program that opened the flash (the XIP flasher), and opening it can rewrite sector 0x80000 (bench/xip/README.md)."; exit 1; }
cmp "$O/full_a.bin" "$O/full_b.bin" && echo "BACKUP OK: two reads match, $(stat -f %z "$O/full_a.bin") bytes" && shasum -a 256 "$O/full_a.bin"
