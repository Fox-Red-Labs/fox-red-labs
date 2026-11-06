#!/bin/sh
# On the board (host): create the frl-show container (Debian trixie arm64) that runs Xorg on the HDMI
# framebuffer plus the showcase apps. About 900 MB of packages from the Debian mirrors.
# Usage: sh setup.sh
set -e
docker rm -f frl-show >/dev/null 2>&1 || true
docker run -d --name frl-show --privileged --net host --ipc host \
  -v /dev:/dev -v /run/udev:/run/udev:ro -v /root/frl:/root/frl -v /tmp/.X11-unix:/tmp/.X11-unix \
  -e DISPLAY=:0 debian:trixie sleep infinity
docker exec frl-show sh -c 'sed -i "s/Components: main/Components: main contrib non-free/" /etc/apt/sources.list.d/debian.sources;
 apt-get update -q && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
 xserver-xorg-core xserver-xorg-video-fbdev xserver-xorg-input-libinput xinit x11-xserver-utils xdotool x11-utils \
 adb meson ninja-build gcc pkg-config libsdl2-dev libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev libswresample-dev libusb-1.0-0-dev git retroarch libretro-gambatte mesa-utils libgl1-mesa-dri libegl1 libgles2 libsdl2-2.0-0 \
 libopenal1 libcurl4 libxrandr2 libxi6 ca-certificates curl procps imagemagick xauth openbox'
docker exec frl-show sh -c 'dpkg -l | grep -E "^ii +(retroarch|scrcpy|adb|libretro-gambatte|xserver-xorg-core) " | awk "{print \$2, \$3}"'
