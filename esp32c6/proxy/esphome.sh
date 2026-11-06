#!/bin/bash
# Run esphome on one of the configs here without a secrets file in the repository.
# The real secrets live outside the tree (SECRETS, default ~/.config/frl/esp32c6/secrets.yaml,
# shape in secrets.example.yaml). Each run copies the configs and the secrets into a build folder outside
# the repository and runs esphome there, so the compile cache (.esphome/) survives between runs.
#
#   esp32c6/proxy/esphome.sh compile frl-ble-proxy.yaml
#   esp32c6/proxy/esphome.sh upload frl-ble-proxy.yaml --device /dev/cu.usbmodemXXXX
#   esp32c6/proxy/esphome.sh logs frl-ble-proxy.yaml --device /dev/cu.usbmodemXXXX
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SECRETS="${SECRETS:-$HOME/.config/frl/esp32c6/secrets.yaml}"
BUILD="${BUILD:-$PWD/out/esp32c6-proxy-build}"
ESPHOME="${ESPHOME:-$HOME/.venvs/esphome/bin/esphome}"
[ -f "$SECRETS" ] || { echo "no secrets file at $SECRETS (copy secrets.example.yaml there and fill it in)"; exit 1; }
[ -x "$ESPHOME" ] || { echo "esphome not found at $ESPHOME (set ESPHOME=...)"; exit 1; }
mkdir -p "$BUILD"
cp "$HERE"/*.yaml "$BUILD"/
cp "$SECRETS" "$BUILD/secrets.yaml"
cd "$BUILD"
exec "$ESPHOME" "$@"
