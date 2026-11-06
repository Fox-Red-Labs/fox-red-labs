#!/bin/sh
# Inside frl-show: build the scrcpy client (there is no arm64 Linux release) against the prebuilt server.
# Usage: docker exec frl-show sh /root/frl/showcase/build_scrcpy.sh
set -e
D=/root/frl/showcase/scrcpy; mkdir -p $D; cd $D
[ -f scrcpy-server-v5.0.1 ] || curl -fsSL -o scrcpy-server-v5.0.1 https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-server-v5.0.1
[ -d src ] || git clone -q --depth 1 -b v5.0.1 https://github.com/Genymobile/scrcpy src
cd src && rm -rf x && meson setup x --buildtype=release --strip -Dprebuilt_server=$D/scrcpy-server-v5.0.1 > ../meson.log
ninja -C x > ../ninja.log && ninja -C x install >/dev/null && scrcpy --version | head -1
