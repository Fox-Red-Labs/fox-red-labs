#!/bin/bash
# Put the house Bluetooth proxy back on the ESP32-C6 (USB serial A0:F2:62:xx:xx:xx) and check Home Assistant.
# If the board is stuck (esptool says "No serial data received"): hold BOOT, tap RESET, release BOOT, then run this.
# The image is the one built from frl-ble-proxy.yaml (ESPHome 2026.9.1): out/esp32c6/esphome/images/proxy.factory.bin
set -eu
IMG=${1:-$PWD/out/esp32c6/esphome/images/proxy.factory.bin}
ESPTOOL=$HOME/.venvs/esp/bin/esptool
PORT=$($HOME/.venvs/esp/bin/python -c "
from serial.tools import list_ports
print(next((p.device for p in list_ports.comports() if p.vid==0x303a and p.serial_number=='A0:F2:62:xx:xx:xx'),''))")
# only the UART cable in: use the CP2102N port that reads this board's MAC (the check is the MAC, never the port name)
if [ -z "$PORT" ]; then for c in /dev/cu.SLAB_USBtoUART /dev/cu.usbserial-*; do [ -e "$c" ] || continue
  if perl -e 'alarm 40; exec @ARGV' $ESPTOOL --port "$c" read-mac 2>&1 | grep -qi "a0:f2:62:xx:xx:xx"; then PORT=$c; break; fi; done; fi
[ -n "$PORT" ] || { echo "board A0:F2:62:xx:xx:xx not found"; exit 2; }
$ESPTOOL --port "$PORT" --after no-reset erase-flash
$ESPTOOL --port "$PORT" --baud 921600 --after hard-reset write-flash 0x0 "$IMG"
cd "$(dirname "$0")/../.."
for i in $(seq 1 18); do
  sleep 10
  v=$(python3 tools/ha.py states sensor.frl_bluetooth_proxy_heap_free | awk '{print $2}')
  echo "$(date +%T) heap free: $v"
  case "$v" in unavailable|"") ;; *) echo "proxy is back in Home Assistant"; exit 0;; esac
done
echo "proxy did not show up in Home Assistant within 3 minutes"; exit 1
