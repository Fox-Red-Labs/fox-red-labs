#!/bin/bash
# Flash a padded .bin to the LP-MSPM0G3519 with DSLite under the probe lock. Usage: flash_look.sh image.bin
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"; IMG="${1:-$PWD/out/chip8/g3519/chip8-g3519.bin}"
DS=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite
python3 "$ROOT/tools/frl/lock.py" exec --serial MG000001 -- "$DS" flash -c "$ROOT/mspm0g3519/MSPM0G3519_MG000001.ccxml" -e -f -v "$IMG,0x0"
