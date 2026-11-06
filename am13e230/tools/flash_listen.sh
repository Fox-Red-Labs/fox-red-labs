#!/bin/bash
# Flash an image and stamp the backchannel output with the Mac's clock: flash_listen.sh <image.out> <seconds> [baud]
# Starts the listener first, so the program's first line is not missed. Prints seconds from the end of the flash command.
HERE="$(cd "$(dirname "$0")/.." && pwd)"; SER="${SER:?set SER to the probe serial}"
D=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
python3 "$HERE/tools/uart_listen.py" "$2" "${3:-115200}" /dev/cu.usbmodem${SER}1 &
LP=$!; sleep 1
"$D" flash -c "$HERE/AM13E23019_$SER.ccxml" -e -f -v -u "$1" > /dev/null 2>&1 || echo "flash failed"
python3 -c "import time;print('flash end at', time.strftime('%H:%M:%S'))"
wait $LP
