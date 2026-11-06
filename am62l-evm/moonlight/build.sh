#!/bin/sh
# Build moonlight-embedded (GPL-3.0, upstream commit f32e415, v2.7.1-4) for arm64 in a Debian trixie container, with
# FFmpeg software decode and SDL output. Ran on the AM62L EVM itself (282 s); the Mac mini's Docker could not pull the base image (registry Bad Gateway):
#   sh build.sh <work dir>      -> <work dir>/out/moonlight and build.log
set -eu
W="$(mkdir -p "${1:?work dir}" && cd "$1" && pwd)"
docker run --rm --network host -v "$W":/w debian:trixie-slim sh -c '
  set -e
  apt-get -qq update >/dev/null
  apt-get -qq install -y --no-install-recommends git ca-certificates build-essential cmake pkg-config \
    libssl-dev libopus-dev libexpat1-dev libavahi-client-dev libcurl4-openssl-dev libevdev-dev libudev-dev \
    libavcodec-dev libavutil-dev libsdl2-dev uuid-dev libasound2-dev libpulse-dev libenet-dev >/dev/null 2>&1
  cd /w; rm -rf src out; git clone -q --recursive https://github.com/moonlight-stream/moonlight-embedded.git src
  cd src; git checkout -q f32e415aea6797d261d6b470dcf8bf18727341c2 && git submodule -q update --init --recursive; git describe --tags --always; mkdir -p build; cd build
  t0=$(date +%s); cmake .. -DCMAKE_BUILD_TYPE=Release > /w/cmake.log 2>&1 || { tail -30 /w/cmake.log; exit 1; }
  grep -iE "ffmpeg|sdl|decoder|found" /w/cmake.log | head -20
  make -j"$(nproc)" > /w/make.log 2>&1 || { tail -30 /w/make.log; exit 1; }
  echo "== build $(( $(date +%s) - t0 )) s on $(nproc) cores"
  mkdir -p /w/out; cp moonlight /w/out/; ls -l /w/out/moonlight; ldd /w/out/moonlight | grep -E "avcodec|SDL" || true
'
