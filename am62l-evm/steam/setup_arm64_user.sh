#!/bin/sh
# On the board (host): the native arm64 Steam client in an Ubuntu 26.04 container, hardened after the security
# review of the first run: Steam runs as an unprivileged user (uid 1000) with its CEF sandbox on, the scripts are
# mounted read-only, and the container uses Docker's default bridge network (no --network host).
#   /root/frl/steam/src  -> /s     (read-only: these scripts, lse_trap/, avshim/)
#   /root/frl/steam/data -> /data  (Steam's home, owned by uid 1000)
# The CEF (Chromium) sandbox needs to create user namespaces; Docker's default seccomp profile blocks
# unshare(CLONE_NEWUSER), so this one container runs with seccomp=unconfined. SANDBOX=0 keeps Docker's profile.
# --shm-size: CEF fills Docker's default 64 MB /dev/shm and the client then dies with SIGBUS in memset.
# Ubuntu 26.04: steamui.so links FFmpeg 8 (libavcodec.so.62); Debian trixie has 7, testing 9.
set -eu
B=/root/frl/steam; N=frl-steam-user
mkdir -p $B/src $B/data; chown 1000:1000 $B/data
SEC=""; [ "${SANDBOX:-1}" = 1 ] && SEC="--security-opt seccomp=unconfined"
docker rm -f $N >/dev/null 2>&1 || true
docker run -d --init --name $N $SEC --shm-size=512m --memory-swap=-1 \
  -v $B/src:/s:ro -v $B/data:/data ubuntu:26.04 sleep infinity
docker exec $N sh -c 'apt-get update -qq'
P="xvfb x11-utils xauth imagemagick mesa-utils libgl1-mesa-dri libegl1 libgbm1 ca-certificates curl unzip file procps
 libsdl3-0 libxtst6 libxinerama1 libxcomposite1 libxdamage1 libxrandr2 libxss1 libxkbcommon0 libibus-1.0-5
 libgtk2.0-0t64 libgtk-3-0t64 libgdk-pixbuf-2.0-0 libpipewire-0.3-0t64 libpulse0 libopenal1 libnm0 libavif16
 libsndfile1 libdbus-1-3 libasyncns0 libavcodec62 libavformat62 libavfilter11 libswresample6 libswscale9 libavutil60
 libnss3 libnspr4 libcups2t64 libatk1.0-0t64 libatk-bridge2.0-0t64 libdrm2 libasound2t64 libusb-1.0-0 libudev1
 fonts-dejavu-core xdg-utils dbus-x11 libsdl2-2.0-0 gcc libc6-dev binutils lsof pciutils libavutil-dev"
docker exec $N sh -c "DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends $(echo $P) >/tmp/apt.log 2>&1; echo apt rc=\$?; tail -2 /tmp/apt.log"
# The helper libraries are built as root into /opt/frl, which the Steam user can read but not change.
docker exec $N sh -c 'mkdir -p /opt/frl && cd /opt/frl &&
  gcc -O2 -march=armv8-a -mno-outline-atomics -fPIC -shared -o lse_trap.so /s/lse_trap/lse_trap.c -ldl &&
  gcc -O2 -march=armv8.1-a -o test_lse /s/lse_trap/test_lse.c -lpthread &&
  gcc -O1 -march=armv8.1-a -o test_patch /s/lse_trap/test_patch.c -lpthread &&
  gcc -O2 -fPIC -shared -Wl,--version-script=/s/avshim/avshim.map -o avshim.so /s/avshim/avshim.c -lavutil && ls -l /opt/frl'
docker exec $N sh -c 'id ubuntu 2>/dev/null || useradd -m -u 1000 steam; getent passwd 1000'   # Ubuntu 26.04 ships user ubuntu as uid 1000
docker exec -u 1000 $N sh -c 'unshare -Ur id 2>&1 | head -1; echo "userns for uid 1000: rc=$?"'
