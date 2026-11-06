#!/bin/bash
# Put the LP-EM-CC2755P10 back to the image it shipped with (TI's data_stream Bluetooth demo) and prove it.
#   cc2755p10/restore.sh [backup folder]  default: the folder you run it from
# Needs, in the run folder:
#   backup-flash-1.bin    main flash 0 to 0xE8000, from the arrival's Minute 0
#   backup-ccfg-1.bin     CCFG, 2 kB at 0x4E020000, from the arrival's Minute 0
#   scfg-sdk-default.bin  SCFG, 1 kB at 0x4E040000. The Minute 0 backup did not read the SCFG, so this is the SCFG of the
#                         F3 SDK 9.21.00.36 drivers/empty example for LP_EM_CC2755P10 (TI Arm Clang build, CRC32 already filled):
#                         no HSM key hash, no debug keys, secure boot off, every permission allowed.
#                         SHA-256 e39761ce57d24e460d49fdcb15f56ca2914664e10c52c51a1afe3cfe3d1517ea.
# Why the SCFG is required: a DSLite flash load starts with a chip erase that also erases the SCFG, and a CC27xx only opens
# its debug port when both CCFG and SCFG are valid (TRM SWCU195A, 9.1.3). An image without an SCFG leaves the chip locked.
# The top 96 kB of flash (0xE8000 to 0xFFFFF, HSM firmware) cannot be read or written by the debugger and is not touched;
# DSLite's default erase keeps it (RetainMainSectors=1, sectors listed in the existing CCFG's chipEraseRetain).
# Writes restore-stock.hex (outside the repo: images can hold keys), flashes it with DSLite, reads flash and CCFG back and compares.
set -euo pipefail
R="${1:-$PWD}"; HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
D=/Applications/ti/uniflash_9.6.0/dslite.sh; C="$HERE/CC2755P10.ccxml"
for f in backup-flash-1.bin backup-ccfg-1.bin scfg-sdk-default.bin; do
  [ -s "$R/$f" ] || { echo "missing $R/$f; see the header of this script" >&2; exit 1; }
done
python3 - "$R" "$REPO" <<'PY'
import sys, struct, zlib; sys.path.insert(0, sys.argv[2] + "/portability/crossbuild")
import run_cc23 as r
R = sys.argv[1]
fl = open(R + "/backup-flash-1.bin", "rb").read().rstrip(b"\xff"); cc = open(R + "/backup-ccfg-1.bin", "rb").read()
sc = open(R + "/scfg-sdk-default.bin", "rb").read()
assert len(cc) == 2048 and len(sc) == 1024, "CCFG must be 2 kB and SCFG 1 kB"
assert struct.unpack_from("<I", sc, 228)[0] == zlib.crc32(sc[:228]), "SCFG CRC32 at offset 228 does not match"
open(R + "/restore-stock.hex", "w").write(r.ihex([(0, fl), (0x4E020000, cc), (0x4E040000, sc)]))
PY
"$D" --mode flash -c "$C" -e -f -u "$R/restore-stock.hex"
sleep 2
# the length given to DSLite memory is in 32-bit words
"$D" --mode memory -c "$C" -r 0x0,0x3A000 -o "$R/after-flash.bin" > /dev/null
"$D" --mode memory -c "$C" -r 0x4E020000,0x200 -o "$R/after-ccfg.bin" > /dev/null
cmp "$R/backup-flash-1.bin" "$R/after-flash.bin" && cmp "$R/backup-ccfg-1.bin" "$R/after-ccfg.bin" && echo "RESTORED: main flash and CCFG read back identical to the Minute 0 backup"
