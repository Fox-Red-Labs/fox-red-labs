#!/bin/bash
# Proof of a running blink on the LP-AM13E230: read GPIO0 DOUT (0x400F1280) bit 5 (LED4, PA5) a few times
# over the probe while the program runs. Changing values mean the LED is toggling.  Usage: led_check.sh [serial] [reads] [gap-s]
HERE="$(cd "$(dirname "$0")/.." && pwd)"; SER="${1:-$(ls "$HERE"/AM13E23019_*.ccxml | head -1 | sed 's/.*_\(.*\)\.ccxml/\1/')}"
N="${2:-6}"; GAP="${3:-0.3}"
D=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
T=$(mktemp -d)
for i in $(seq 1 "$N"); do
  "$D" memory -c "$HERE/AM13E23019_$SER.ccxml" -r 0x400F1280,1 -o "$T/r$i.bin" > "$T/log$i" 2>&1
  python3 -c "import sys;d=open('$T/r$i.bin','rb').read(4);v=int.from_bytes(d,'little');print('$(date +%H:%M:%S)', hex(v), 'PA5', (v>>5)&1)"
  sleep "$GAP"
done
rm -rf "$T"
