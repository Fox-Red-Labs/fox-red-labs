#!/bin/sh
# On the board (host): Valve's steamcmd (32-bit x86 only) under Box64's BOX32 in a Debian trixie arm64 container,
# anonymous login, one anonymous-allowed app. No account. Usage: sh steamcmd_box64.sh [appid]
# Default app 1007 (Steamworks SDK Redist, about 100 MB), allowed for anonymous login; 896660 is the Valheim server.
set -u
APP=${1:-1007}; B=/root/frl/games; mkdir -p $B/steamcmd $B/apps $B/logs
docker rm -f frl-steamcmd >/dev/null 2>&1
docker run -d --init --name frl-steamcmd -v $B:/g debian:trixie sleep infinity >/dev/null
docker exec frl-steamcmd sh -c 'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends box64 curl ca-certificates procps >/g/logs/apt-steamcmd.log 2>&1; box64 --version 2>&1 | head -1'
docker exec frl-steamcmd sh -c 'cd /g/steamcmd && [ -s steamcmd.sh ] || curl -fsSL https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz | tar xz; file linux32/steamcmd 2>/dev/null; ls'
docker exec frl-steamcmd sh -c "cd /g/steamcmd && export HOME=/g/home; mkdir -p \$HOME; s=\$(date +%s);
  BOX64_LOG=0 box64 ./linux32/steamcmd +force_install_dir /g/apps/$APP +login anonymous +app_update $APP validate +quit > /g/logs/steamcmd-$APP.log 2>&1;
  echo rc=\$? seconds=\$(( \$(date +%s)-s )); du -sm /g/apps/$APP; tail -5 /g/logs/steamcmd-$APP.log"
