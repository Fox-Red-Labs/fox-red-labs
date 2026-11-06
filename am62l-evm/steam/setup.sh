#!/bin/sh
# Steam long shot on the AM62L EVM: a long-lived Debian trixie arm64 container "frl-steam" with Box64 0.3.4 (Debian
# package), Xvfb and Mesa llvmpipe. On this board both `docker build` (legacy builder, no buildx) and `docker commit`
# sat for minutes at 0 percent CPU, so the packages go into a running container instead. Run on the board:
#   sh setup.sh        then: docker exec frl-steam sh /s/run_steam.sh
set -eu
docker rm -f frl-steam >/dev/null 2>&1 || true
docker run -d --name frl-steam --network host -v /root/frl/steam:/s debian:trixie-slim sleep infinity
docker exec frl-steam sh -c 'apt-get -qq update && DEBIAN_FRONTEND=noninteractive apt-get -qq install -y \
  --no-install-recommends box64 xvfb x11-apps xauth mesa-utils libgl1-mesa-dri ca-certificates curl xz-utils file \
  procps imagemagick binutils >/dev/null 2>&1'
