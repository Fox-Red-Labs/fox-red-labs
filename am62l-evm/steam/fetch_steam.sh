#!/bin/sh
# Download Valve's Steam installer package and unpack its bootstrap (no account, nothing signed in).
# Run inside the container: sh /s/fetch_steam.sh /s/work
set -eu
W="${1:?work dir}"; mkdir -p "$W"; cd "$W"
curl -fsSL -o steam_latest.deb https://repo.steampowered.com/steam/archive/stable/steam_latest.deb
ar x steam_latest.deb && tar xf data.tar.* && ls usr/lib/steam
mkdir -p boot && tar xf usr/lib/steam/bootstraplinux_ubuntu12_32.tar.xz -C boot
file boot/ubuntu12_32/steam boot/steam.sh | sed "s|$W/||"
