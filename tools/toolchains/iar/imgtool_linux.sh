#!/bin/sh
# Linux stand-in for the SimpleLink SDK's tools/common/mcuboot/imgtool (a macOS binary): runs MCUboot's own Python
# imgtool from the same SDK (source/third_party/mcuboot/scripts) with Linux wheels of its dependencies from
# $FRL_PYDEPS (default $FRL_TI_ROOT/pydeps-linux; installed on the build host with
#   pip install --target <dir> --platform manylinux2014_x86_64 --python-version 3.12 --implementation cp --abi cp312 \
#       --only-binary=:all: cryptography intelhex click cbor2 pyyaml ).
SDK=$(cd "$(dirname "$0")/../../.." && pwd)
PYTHONPATH="${FRL_PYDEPS:-${FRL_TI_ROOT:-/opt/ti}/pydeps-linux}:$SDK/source/third_party/mcuboot/scripts" exec python3 "$SDK/source/third_party/mcuboot/scripts/imgtool.py" "$@"
