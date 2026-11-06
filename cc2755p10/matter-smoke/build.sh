#!/usr/bin/env bash
# Run ON the Mac mini (needs docker). Builds TI's CC27xx Matter apps for LP_EM_CC2755P10 inside ubuntu:22.04 amd64,
# reusing docker volume frl-matter-build (/build/matter = tag matter-v1.5-ti-1.2-ea.2, /build/ti = toolchains).
# usage: [GN_EXTRA='chip_enable_icd_server=true chip_openthread_ftd=false' OUTN=smoke-icd] build.sh PHASE   PHASE = sub (fetch cc27xx submodules) | sdk (build F3 SDK libraries) | light (lighting app) | smoke (smoke-co-alarm app)
# Output goes to /build/out-t in the volume; the work folder ~/frl-matter/work-t gets copies of the images.
set -euo pipefail
PHASE="${1:?phase: sub|light|smoke}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK=$HOME/frl-matter/work-t; mkdir -p "$WORK"
rm -rf "$WORK/overlay"; cp -r "$HERE"/overlay "$WORK"/
docker run --rm --platform linux/amd64 -v "$WORK":/work -v frl-matter-build:/build -e PHASE=$PHASE -e GN_EXTRA="${GN_EXTRA:-}" -e OUTN="${OUTN:-$PHASE}" ubuntu:22.04 bash -lc '
set -eo pipefail
export DEBIAN_FRONTEND=noninteractive
t0=$(date +%s); step(){ echo "=== $(( $(date +%s)-t0 ))s  $*"; }
step apt
apt-get update -qq
apt-get install -y -qq git gcc g++ pkg-config libssl-dev libdbus-1-dev libglib2.0-dev libavahi-client-dev ninja-build python3-venv python3-dev python3-pip unzip libgirepository1.0-dev libcairo2-dev libreadline-dev curl ca-certificates xz-utils make sudo libusb-1.0-0 libncurses5 libxkbcommon0 >/dev/null
mkdir -p /root; ln -sfn /build/ti /root/ti
cd /build/matter
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0="*"
set +u; source ./scripts/activate.sh; set -u
case "$PHASE" in
sub)
  step submodules
  ./scripts/checkout_submodules.py --shallow --platform cc27xx --recursive
  du -sh third_party/ti_simplelink_sdk/repo_cc23xx_cc27xx third_party/openthread/ot-ti
  ;;
sdk)
  step sdklibs
  cd third_party/ti_simplelink_sdk/repo_cc23xx_cc27xx
  sed -i "s|^SYSCONFIG_TOOL .*|SYSCONFIG_TOOL ?= /root/ti/sysconfig_1.28.1/sysconfig_cli.sh|; s|^CMAKE .*|CMAKE ?= /root/ti/cmake-3.21.3-linux-x86_64/bin/cmake|; s|^GCC_ARMCOMPILER .*|GCC_ARMCOMPILER ?= /root/ti/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi|" imports.mak
  export PATH=/root/ti/cmake-3.21.3-linux-x86_64/bin:$PATH
  make GENERATOR=Ninja build-gcc
  ;;
light|smoke)
  if [ "$PHASE" = smoke ]; then
    step overlay
    rm -rf examples/smoke-co-alarm-app/ti; mkdir -p examples/smoke-co-alarm-app/ti
    cp -r examples/lighting-app/ti/cc27xx examples/smoke-co-alarm-app/ti/cc27xx
    DIR=examples/smoke-co-alarm-app/ti/cc27xx
    cp -r /work/overlay/src/. $DIR/src/
    python3 /work/overlay/patch_apptask.py $DIR/src/AppTask.cpp
    sed -i "s|examples/lighting-app/ti/cc27xx|examples/smoke-co-alarm-app/ti/cc27xx|; s|lighting-example|smoke-co-alarm-example|; s|\${chip_root}/examples/lighting-app/lighting-common|\${chip_root}/examples/smoke-co-alarm-app/smoke-co-alarm-common|; s|\"\${project_dir}/src/ZclCallbacks.cpp\",|\"\${project_dir}/src/ZclCallbacks.cpp\", \"\${project_dir}/src/SmokeAlarm.cpp\",|" $DIR/BUILD.gn
    grep -n "smoke\|Smoke" $DIR/BUILD.gn
  else
    DIR=examples/lighting-app/ti/cc27xx
  fi
  cd $DIR
  step gn
  gn gen out-t/$OUTN --args="ti_sysconfig_root=\"/root/ti/sysconfig_1.28.1\" ti_simplelink_board=\"LP_EM_CC2755P10\" $GN_EXTRA"
  step ninja
  ninja -C out-t/$OUTN
  step done
  ls -la out-t/$OUTN/*.out out-t/$OUTN/*.map 2>/dev/null || true
  /build/matter/.environment/cipd/packages/arm/bin/arm-none-eabi-size out-t/$OUTN/*.out || true
  mkdir -p /work/out/$OUTN; cp out-t/$OUTN/*.out out-t/$OUTN/*.map /work/out/$OUTN/ 2>/dev/null || true
  ;;
esac
step end
'
