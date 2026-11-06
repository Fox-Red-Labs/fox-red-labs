#!/bin/sh
# Inside frl-steam-user, as uid 1000: share the Xvfb screen (:1) through x11vnc on a unix socket only, with a
# one-time password (8 characters, the VNC limit) printed once. No TCP port. Usage: sh /s/vnc_start.sh
# Env: SCALE=WxH scales the shared screen (1728x1080 fills a 1080p panel at the 1280x800 aspect).
set -eu
pkill -u "$(id -u)" -x x11vnc 2>/dev/null || true; mkdir -p /data/vnc; chmod 700 /data/vnc; rm -f /data/vnc/vnc.sock
PW=$(head -c 4096 /dev/urandom | tr -dc 'a-km-z2-9' | head -c 8)
x11vnc -storepasswd "$PW" /data/vnc/passwd >/dev/null 2>&1
nohup x11vnc -display :1 -rfbport 0 -unixsock /data/vnc/vnc.sock -rfbauth /data/vnc/passwd -forever -shared \
  -noxdamage ${SCALE:+-scale $SCALE} -quiet > /data/vnc/x11vnc.log 2>&1 &
sleep 5; ls -l /data/vnc/vnc.sock >/dev/null && echo "x11vnc on /data/vnc/vnc.sock, one-time password: $PW"
