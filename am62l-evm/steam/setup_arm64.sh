#!/bin/sh
# On the board (host): an Ubuntu 26.04 arm64 container for Valve's native arm64 Steam client.
# Ubuntu 26.04, not Debian trixie or testing: steamui.so links FFmpeg 8 (libavcodec.so.62); trixie has 7, testing 9. Packages go into the running
# container (docker build and docker commit stalled on this board). Bind mount /root/frl/steam -> /s.
# --shm-size: CEF fills Docker's default 64 MB /dev/shm and the client then dies with SIGBUS in memset.
set -eu
docker rm -f frl-steam-arm64 >/dev/null 2>&1 || true
docker run -d --name frl-steam-arm64 --network host --shm-size=512m -v /root/frl/steam:/s ubuntu:26.04 sleep infinity
docker exec frl-steam-arm64 sh -c 'cat /etc/debian_version; apt-get update -qq'
P="xvfb x11-utils xauth imagemagick mesa-utils libgl1-mesa-dri libegl1 libgbm1 ca-certificates curl unzip file procps
 libsdl3-0 libxtst6 libxinerama1 libxcomposite1 libxdamage1 libxrandr2 libxss1 libxkbcommon0 libibus-1.0-5
 libgtk2.0-0t64 libgtk-3-0t64 libgdk-pixbuf-2.0-0 libpipewire-0.3-0t64 libpulse0 libopenal1 libnm0 libavif16
 libsndfile1 libdbus-1-3 libasyncns0 libavcodec62 libavformat62 libavfilter11 libswresample6 libswscale9 libavutil60
 libnss3 libnspr4 libcups2t64 libatk1.0-0t64 libatk-bridge2.0-0t64 libdrm2 libasound2t64 libusb-1.0-0 libudev1
 fonts-dejavu-core xdg-utils dbus-x11 libsdl2-2.0-0 gcc libc6-dev binutils lsof pciutils"
docker exec frl-steam-arm64 sh -c "DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends $(echo $P) >/s/arm64/apt.log 2>&1; echo apt rc=\$?; tail -3 /s/arm64/apt.log"
docker exec frl-steam-arm64 sh -c 'R=/s/arm64/home/.local/share/Steam/steamrtarm64; cd $R; echo "still missing:"; for f in steam steamwebhelper *.so; do LD_LIBRARY_PATH=$R:$R/libs ldd $f 2>/dev/null | grep "not found"; done | sort -u'
