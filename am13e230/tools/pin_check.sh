#!/bin/bash
# Read one 32-bit register over the probe several times and print the chosen bit: pin_check.sh <addr> <bit> [reads] [gap-s]
# Changing values mean the pin toggles. GPIO0 DOUT is 0x400F1280 (PA0-PA31), GPIO1 DOUT is 0x400F3280 (PB0-).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; N="${3:-6}"; GAP="${4:-0.15}"
D=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
T=$(mktemp -d)
for i in $(seq 1 "$N"); do
  "$D" memory -c "$HERE/AM13E23019_AL130001.ccxml" -r "$1,4" -o "$T/r.bin" > /dev/null 2>&1
  python3 -c "import time;d=open('$T/r.bin','rb').read(4);v=int.from_bytes(d,'little');print('%.2f'%time.time(), hex(v), 'bit $2 =', (v>>$2)&1)"
  sleep "$GAP"
done
rm -rf "$T"
