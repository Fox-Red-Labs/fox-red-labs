#!/bin/bash
# M0: build the OpenThread border-router host libraries for the CC35xx Cortex-M33 (GCC from the Zephyr SDK,
# CMake below 4 from ~/ot-ti/_tools, OpenThread from ~/ot-ti/openthread). Usage: build.sh <build dir>
set -e
B="${1:?build dir}"; HERE="$(cd "$(dirname "$0")" && pwd)"
export PATH=$HOME/ot-ti/_tools/gcc/bin:$HOME/ot-ti/_tools/venv/bin:$PATH
cd $HOME/ot-ti/openthread
cmake -GNinja -B "$B" -DCMAKE_TOOLCHAIN_FILE=$HOME/ot-ti/src/arm-none-eabi-m33.cmake -DOT_PLATFORM=external \
  -DOT_FTD=ON -DOT_MTD=OFF -DOT_RCP=OFF -DOT_APP_CLI=OFF -DOT_APP_NCP=OFF -DOT_APP_RCP=OFF \
  -DOT_BORDER_ROUTING=ON -DOT_SRP_SERVER=ON -DOT_DNSSD_SERVER=ON -DOT_MDNS=ON -DOT_NAT64_BORDER_ROUTING=ON \
  -DOT_NAT64_TRANSLATOR=ON -DOT_BORDER_AGENT=ON -DOT_NETDATA_PUBLISHER=ON -DOT_COMPILE_WARNING_AS_ERROR=OFF \
  -DOT_PROJECT_CONFIG="$HERE/openthread-core-cc35xx-config.h" .
rm -f "$B/third_party/mbedtls/openthread-mbedtls-config.h"   # generated once; stale after config edits
cmake --build "$B" --target openthread-ftd openthread-cli-ftd openthread-radio-spinel openthread-hdlc openthread-url mbedtls
