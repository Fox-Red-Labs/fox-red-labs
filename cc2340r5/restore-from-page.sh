#!/bin/bash
# Put back the LP-EM-CC2340R5 from the copies the browser bench saved before it wrote (site/flash/bench/), and prove it.
#   cc2340r5/restore-from-page.sh <folder>      run from the repository root
# The folder holds the page's two files for one save: <board>-<time>-main.bin (512 kB from 0x0) and
# <board>-<time>-ccfg.bin (2 kB at 0x4E020000). Steps are the board's documented put-back (cc2340r5/AGENTS.md,
# Restore): merge them into one hex, load it with DSLite (the default load erases the chip first), then read main flash
# and CCFG back and compare them byte for byte with the page's files. Writes restore.hex and the read-backs into <folder>
# (outside the repo: images can hold a Zigbee key). Needs UniFlash 9.6.0; the probe serial is in CC2340R5.ccxml.
set -euo pipefail
R="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"
DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite; C="$HERE/CC2340R5.ccxml"
MAIN=$(ls "$R"/*-main.bin | head -1); CCFG=$(ls "$R"/*-ccfg.bin | head -1)
[ "$(stat -f %z "$MAIN")" = 524288 ] || { echo "$MAIN is not 512 kB" >&2; exit 1; }
[ "$(stat -f %z "$CCFG")" = 2048 ] || { echo "$CCFG is not 2 kB" >&2; exit 1; }
python3 "$HERE/oad/images.py" merge "$R/restore.hex" "$MAIN@0x0" "$CCFG@0x4E020000"
"$DSLITE" flash -c "$C" -e -f -u "$R/restore.hex" > "$R/restore-flash.log" 2>&1 || { tail -5 "$R/restore-flash.log"; exit 1; }
sleep 2
# the length given to DSLite memory is in 32-bit words
"$DSLITE" memory -c "$C" -r 0x0,0x20000 -o "$R/after-main.bin" > /dev/null
"$DSLITE" memory -c "$C" -r 0x4E020000,0x200 -o "$R/after-ccfg.bin" > /dev/null
# The restored program starts at once and may write its own storage (TI's NVS, the top 4 kB, 0x7F000 to 0x7FFFF, where a
# Zigbee image keeps its network) before the read-back: changes there are reported, anything else fails.
python3 - "$MAIN" "$R/after-main.bin" "$CCFG" "$R/after-ccfg.bin" <<'PY2'
import sys
a, b, c, d = (open(x, "rb").read() for x in sys.argv[1:5])
out = [i for i in range(len(a)) if a[i] != b[i] and not 0x7F000 <= i < 0x80000]
nvs = sum(1 for i in range(0x7F000, 0x80000) if a[i] != b[i])
if out or c != d:
    sys.exit(f"NOT RESTORED: {len(out)} bytes differ outside the NVS area (first at {out[0]:#x})" if out else "NOT RESTORED: CCFG differs")
print(f"RESTORED: main flash and CCFG read back identical to the page's copies" + (f", except {nvs} bytes the program wrote to its NVS area after it started" if nvs else ""))
PY2
