#!/bin/sh
# Build Box64 from current git with BOX32 for an ARMv8.0 Cortex-A53 (AM62L), in a Debian trixie arm64
# container on an arm64 build host. No -mcpu=native: the build host's CPU is newer than the A53, and Box64's
# dynarec picks CPU features (LSE, etc.) at run time. Output: $OUT/box64-git-<sha>.tar.gz
# Usage (build host, or the board itself): OUT=/path [IMAGE=debian:trixie] sh build_box64_git.sh
set -eu
OUT=${OUT:?output dir}; mkdir -p "$OUT"
docker run --rm --network host -v "$OUT":/out "${IMAGE:-debian:trixie-slim}" sh -c '
set -e
apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
  git ca-certificates cmake make gcc g++ python3 >/dev/null
git clone -q --depth 1 https://github.com/ptitSeb/box64 /b && cd /b
SHA=$(git rev-parse --short HEAD); echo "box64 git $SHA $(git log -1 --format=%cI)"
grep -n "__sched_cpualloc" src/wrapped32/wrappedlibc_private.h
mkdir build && cd build
cmake .. -DARM_DYNAREC=ON -DBOX32=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-march=armv8-a" -DCMAKE_CXX_FLAGS="-march=armv8-a" >/out/cmake.log 2>&1
t0=$(date +%s); make -j"$(nproc)" >/out/make.log 2>&1; echo "make: $(( $(date +%s)-t0 )) s on $(nproc) cores"
./box64 --version
mkdir -p /pkg/usr/local/bin /pkg/etc && cp box64 /pkg/usr/local/bin/ && cp ../system/box64.box64rc /pkg/etc/box64.box64rc
mkdir -p /pkg/usr/lib/box64-x86_64-linux-gnu /pkg/usr/lib/box64-i386-linux-gnu
cp -a ../x64lib/* /pkg/usr/lib/box64-x86_64-linux-gnu/ 2>/dev/null || true
cp -a ../x86lib/* /pkg/usr/lib/box64-i386-linux-gnu/ 2>/dev/null || true
tar czf /out/box64-git-$SHA.tar.gz -C /pkg . && ls -l /out/box64-git-$SHA.tar.gz
'
