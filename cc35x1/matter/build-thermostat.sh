#!/usr/bin/env bash
# Builds the Fox Red Labs Matter THERMOSTAT image (Thermostat endpoint 1, Occupancy Sensing endpoint 2) for the LP-EM-CC35X1 in a throwaway Ubuntu 22.04 amd64 container.
# Same recipe and pins as build.sh, but the tool installs, the source tree and the bootstrap live in the Docker volume frl-matter-build so a second run only redoes the app (about 4 minutes). Remove the volume afterwards: docker volume rm frl-matter-build
# Run it ON a Docker host:  ./build-thermostat.sh /path/to/work   (the folder holding this script and thermostat/ is mounted at /overlay)
# FRL_CSI=1 adds TI's Wi-Fi CSI presence pipeline (thermostat/csi/); FRL_HEAP=0x51000 overrides the FreeRTOS heap size.
# Needs a case-sensitive filesystem for the source tree, so it builds in a Docker volume (frl-matter-build); remove it afterwards: docker volume rm frl-matter-build
# Run it ON a Linux or Docker host:  ./build.sh /path/to/work
# Public sources only. Versions pinned from the TI Matter release notes (read 2026-10-09).
set -euo pipefail
WORK="${1:?work folder}"; mkdir -p "$WORK"
TAG=matter-v1.5-ti-1.2-ea.2
docker run --rm --platform linux/amd64 -v "$WORK":/work -v "$(cd "$(dirname "$0")" && pwd)":/overlay:ro -v frl-matter-build:/build -e TAG=$TAG -e FRL_CSI="${FRL_CSI:-}" -e FRL_CSI_TRIM="${FRL_CSI_TRIM:-}" -e FRL_HEAP="${FRL_HEAP:-}" ubuntu:22.04 bash -lc '
set -eo pipefail
export DEBIAN_FRONTEND=noninteractive
t0=$(date +%s); step(){ echo "=== $(( $(date +%s)-t0 ))s  $*"; }
step apt
apt-get update -qq
apt-get install -y -qq git gcc g++ pkg-config libssl-dev libdbus-1-dev libglib2.0-dev libavahi-client-dev ninja-build python3-venv python3-dev python3-pip unzip libgirepository1.0-dev libcairo2-dev libreadline-dev curl ca-certificates xz-utils make sudo libusb-1.0-0 libncurses5 libxkbcommon0 >/dev/null
mkdir -p /work/dl /build/ti; ln -sfn /build/ti /root/ti
cd /work/dl
step tools
[ -f cmake.tgz ] || curl -fsSL -o cmake.tgz https://github.com/Kitware/CMake/releases/download/v3.21.3/cmake-3.21.3-linux-x86_64.tar.gz
[ -f gcc.txz ] || curl -fsSL -o gcc.txz https://developer.arm.com/-/media/Files/downloads/gnu/12.3.rel1/binrel/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
[ -f sysconfig.run ] || curl -fsSL -o sysconfig.run https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.28.1.4785/sysconfig-1.28.1_4785-setup.run
[ -f toolbox.run ] || curl -fsSL -o toolbox.run https://dr-download.ti.com/software-development/application-software-framework/MD-xUhL0Nqy3m/4.3.23/simplelink_wifi_toolbox_lin_4_3_23.run
# SHA-256 from each vendor page (Kitware cmake-3.21.3-SHA-256.txt, Arm .sha256asc, ti.com/tool/SYSCONFIG and /tool/SIMPLELINK-WIFI-TOOLBOX), read 2026-10-09
sha256sum -c - <<X
a19aa9fcf368e9d923cdb29189528f0fe00a0d08e752ba4e547af91817518696  cmake.tgz
12a2815644318ebcceaf84beabb665d0924b6e79e21048452c5331a56332b309  gcc.txz
b913ca843bf0dff177dc4c22dee2e73b9bdc129272c00a64033e3e7ee0e9f55b  sysconfig.run
111b6a0ddcd77b2a5c5d16c4041704e482daa2ec253d7b489eed9c29ff87cf0e  toolbox.run
X
[ -d /root/ti/cmake-3.21.3-linux-x86_64 ] || tar xzf cmake.tgz -C /root/ti
[ -d /root/ti/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi ] || tar xJf gcc.txz -C /root/ti
chmod +x *.run
[ -d /root/ti/sysconfig_1.28.1 ] || ./sysconfig.run --mode unattended --prefix /root/ti/sysconfig_1.28.1
[ -d /root/ti/simplelink_wifi_toolbox_lin_4_3_23 ] || ./toolbox.run --mode unattended --prefix /root/ti
ls /root/ti
mkdir -p /build; cd /build
step clone
# pinned to the commit the tag pointed at on 2026-10-09
COMMIT=33a45a1ba707418ee9f764723a5b98f2718e8abd
if [ ! -d matter ]; then git init -q matter; cd matter; git remote add origin https://github.com/TexasInstruments/matter.git; git fetch -q --depth 1 origin $COMMIT; git checkout -q FETCH_HEAD; cd ..; fi
cd matter
test "$(git rev-parse HEAD)" = "$COMMIT"
step submodules
[ -f /build/.submodules.done ] || { ./scripts/checkout_submodules.py --shallow --platform cc35xx linux --recursive; touch /build/.submodules.done; }
step bootstrap
# bluezoo (Linux BLE test helper, not needed for a firmware build) is not installable from PyPI today
sed -i "/bluezoo/d" scripts/tests/requirements.txt
source ./scripts/bootstrap.sh -p ti
step sdklibs
if [ ! -f /build/.sdklibs.done ]; then cd third_party/ti_simplelink_sdk/repo_cc35xx
cat > imports.mak.local <<X
X
sed -i "s|^SYSCONFIG_TOOL.*|SYSCONFIG_TOOL ?= /root/ti/sysconfig_1.28.1/sysconfig_cli.sh|; s|^SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR.*|SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR ?= /root/ti/simplelink_wifi_toolbox_lin_4_3_23|; s|^CMAKE .*|CMAKE ?= /root/ti/cmake-3.21.3-linux-x86_64/bin/cmake|; s|^GCC_ARMCOMPILER.*|GCC_ARMCOMPILER ?= /root/ti/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi|" imports.mak
export PATH=/root/ti/cmake-3.21.3-linux-x86_64/bin:$PATH
make GENERATOR=Ninja build-gcc
touch /build/.sdklibs.done; fi
cd /build/matter
step app
source ./scripts/activate.sh
python3 /overlay/thermostat/make_example.py
# the .matter file the build reads is generated from the .zap by TI-pinned ZAP
python3 scripts/tools/zap/generate.py examples/thermostat/ti/cc35xx/data_model/thermostat-app.zap -t src/app/zap-templates/matter-idl-server.json
ls -la examples/thermostat/ti/cc35xx/data_model
cd examples/thermostat/ti/cc35xx
LDSARG=""; [ "$FRL_CSI" = "1" ] && LDSARG=" frl_cc35xx_ldscript=\"/build/matter/examples/thermostat/ti/cc35xx/cc35xx_freertos.lds\""
gn gen out/debug --args="ti_sysconfig_root=\"/root/ti/sysconfig_1.28.1\" ti_simplelink_wifi_toolbox_root=\"/root/ti/simplelink_wifi_toolbox_lin_4_3_23\"$LDSARG"
ninja -C out/debug
step done
rm -rf /work/out-thermostat; mkdir -p /work/out-thermostat; cp -r out/debug /work/out-thermostat/
ls -laR out/debug | head -80
'
