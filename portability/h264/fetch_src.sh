#!/bin/bash
# fetch_src.sh [dir]: clone h264bsd at the pinned commit (once), copy its src/ to <dir>/src-patched and apply h264bsd-cfg.sed there.
# The upstream clone stays untouched; the patch only lowers the parameter-set table sizes (see SOURCE.md). Prints the patched src path.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; W="${W:-$PWD/out/h264/work}"; mkdir -p "$W"
[ -d "$W/h264bsd" ] || git clone -q https://github.com/oneam/h264bsd.git "$W/h264bsd"
git -C "$W/h264bsd" checkout -q 42bcb5d753ad86d84903354bf3c68423c28adb7b
rm -rf "$W/patched"; mkdir -p "$W/patched/h264bsd"; cp -R "$W/h264bsd/src" "$W/patched/h264bsd/src"
if [ "${UNPATCHED:-0}" != 1 ]; then sed -i.bak -f "$HERE/h264bsd-cfg.sed" "$W/patched/h264bsd/src/h264bsd_cfg.h"; fi
echo "$W/patched/h264bsd/src"
