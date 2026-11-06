#!/bin/bash
# runs inside ubuntu:22.04 (amd64); work folder mounted at /work
set -euxo pipefail
export DEBIAN_FRONTEND=noninteractive
T0=$(date +%s)
apt-get update -qq
apt-get install -y -qq sudo git wget ca-certificates lsb-release xz-utils bzip2 python3 build-essential cmake ninja-build unzip file >/dev/null
mkdir -p /work/dl
# TI's SysConfig installer runs as root in this container, so: TLS checked, and the file must match SC_SHA256.
# Set SC_SHA256 to the SHA-256 published on TI's SysConfig download page (or the value recorded in README.md after a
# verified download). With no pin the script prints the file's hash and stops, so nothing unverified ever runs.
[ -f /work/dl/sc.run ] || wget -q -O /work/dl/sc.run https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.27.0.4565/sysconfig-1.27.0_4565-setup.run
GOT=$(sha256sum /work/dl/sc.run | cut -d' ' -f1)
if [ -z "${SC_SHA256:-}" ]; then echo "sysconfig installer sha256 $GOT: set SC_SHA256 after checking it against TI's page, then rerun"; exit 3; fi
[ "$GOT" = "$SC_SHA256" ] || { echo "sysconfig installer hash mismatch: got $GOT, want $SC_SHA256"; rm -f /work/dl/sc.run; exit 4; }
chmod +x /work/dl/sc.run
[ -d /opt/ti/sysconfig_1.27.0 ] || /work/dl/sc.run --mode unattended
export TI_SYSCONFIG_ROOT=/opt/ti/sysconfig_1.27.0
cd /work
if [ ! -d ot-ti ]; then
  git clone -q --branch thread-v1.4-ti-2.1 https://github.com/TexasInstruments/ot-ti.git
fi
cd ot-ti
# pin to the commit the release tag pointed at when we built (2026-10-09), so a moved tag cannot change what runs
git -c advice.detachedHead=false checkout -q eaac0deb4b3369742f43416c26531cc16938d8c6
git submodule update --init
echo "T_CLONE=$(( $(date +%s)-T0 ))"
./script/bootstrap
echo "T_BOOTSTRAP=$(( $(date +%s)-T0 ))"
T1=$(date +%s)
export GCC_ARMCOMPILER=/opt/arm-gnu-toolchain-12.2.rel1-$(arch)-arm-none-eabi
./script/build LP_EM_CC2755P10
echo "T_BUILD_ALL=$(( $(date +%s)-T1 ))"
git rev-parse HEAD; git submodule status; ls -l build/bin
arm-none-eabi-gcc --version | head -1
for f in build/bin/*; do arm-none-eabi-size "$f"; done
