#!/usr/bin/env bash
# Install the room sensors on a Debian-family Linux host (BeagleY-AI, Raspberry Pi, any PC). Run as root.
#   tar xzf room-host.tgz && sudo iwrl6432boost/live/host/install.sh [--dry-run]
# Puts the code in /opt/room-sensors, a Python venv beside it, the logs in /var/lib/room-sensors,
# settings in /etc/room-sensors.env (root only), and two systemd services that are installed but not started.
set -euo pipefail
DRY=${1:-}
run() { if [ "$DRY" = "--dry-run" ]; then echo "+ $*"; else "$@"; fi; }
SRC=$(cd "$(dirname "$0")/../../.." && pwd)
DEST=/opt/room-sensors
run mkdir -p "$DEST" /var/lib/room-sensors/radar /var/lib/room-sensors/wifi
run cp -R "$SRC/iwrl6432boost" "$SRC/tools" "$SRC/cc35x1" "$DEST/"
command -v python3 >/dev/null || { echo "needs python3 (3.10 or newer)"; exit 1; }
python3 -c 'import venv' 2>/dev/null || run apt-get install -y python3-venv
run python3 -m venv "$DEST/venv"
run "$DEST/venv/bin/pip" install --quiet pyserial numpy
if [ ! -f /etc/room-sensors.env ]; then
  run install -m 600 "$SRC/iwrl6432boost/live/host/room-sensors.env.example" /etc/room-sensors.env
fi
run cp "$SRC/iwrl6432boost/live/host/radar-room.service" "$SRC/iwrl6432boost/live/host/wifi-room.service" /etc/systemd/system/
run systemctl daemon-reload
cat <<MSG

Installed. Next:
  1. Edit /etc/room-sensors.env: the probe serials, this host's LAN address, Home Assistant URL and token.
  2. Plug in the radar and run once by hand to check: $DEST/venv/bin/python $DEST/iwrl6432boost/live/radar_live.py --selftest
  3. sudo systemctl enable --now radar-room     (and wifi-room for the Wi-Fi board)
  4. Open http://<LAN address>:8765/ (radar) and :8820/ (Wi-Fi). Review page: /review. Logs: /var/lib/room-sensors.
MSG
