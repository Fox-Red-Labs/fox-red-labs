#!/usr/bin/env bash
# Bluetooth light for the LP-EM-CC2340R5. The source and the build live in cc2755p10/ble-light (one copy for both boards).
exec "$(cd "$(dirname "$0")" && pwd)/../../cc2755p10/ble-light/build.sh" cc2340r5
