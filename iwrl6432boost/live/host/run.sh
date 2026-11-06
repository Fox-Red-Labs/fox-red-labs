#!/usr/bin/env bash
# Start one room sensor with the settings in /etc/room-sensors.env. Used by the systemd services.
#   run.sh radar | run.sh wifi
set -euo pipefail
ROOT=/opt/room-sensors
PY=$ROOT/venv/bin/python
HA=(); [ -n "${HA_URL:-}" ] && [ -n "${HA_TOKEN:-}" ] && HA=(--ha)
BIND=(); [ -n "${LAN_ADDR:-}" ] && [ "$LAN_ADDR" != 127.0.0.1 ] && BIND=(--bind "$LAN_ADDR")
case "$1" in
  radar)
    ROOM=$ROOT/iwrl6432boost/live/room-learned.json; [ -f "$ROOM" ] || ROOM=$ROOT/iwrl6432boost/live/room.json
    CFG=$ROOT/iwrl6432boost/live/cfg/calibrated.cfg; [ -f "$CFG" ] || CFG=$ROOT/iwrl6432boost/live/cfg/people-count-250ms.cfg
    exec "$PY" "$ROOT/iwrl6432boost/live/radar_live.py" --probe "${RADAR_PROBE:-RI32}" --cfg "$CFG" --room "$ROOM" \
      --log /var/lib/room-sensors/radar --learn-every 900 "${HA[@]}" "${BIND[@]}" ;;
  wifi)
    # Every start joins the network (FRL_WIFI_SSID, FRL_WIFI_PSK): a host reboot power-cycles the board on its USB.
    exec "$PY" "$ROOT/cc35x1/csi/lab/server.py" --port "$WIFI_PORT" --local-only \
      --log /var/lib/room-sensors/wifi "${HA[@]}" "${BIND[@]}" ;;
  *) echo "usage: run.sh radar|wifi"; exit 2 ;;
esac
