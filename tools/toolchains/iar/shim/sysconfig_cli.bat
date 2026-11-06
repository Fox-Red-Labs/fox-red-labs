#!/bin/sh
# Stand-in for SysConfig inside the Linux container. The SDK's IAR projects call
# "$SYSCONFIG_ROOT$/sysconfig_cli.bat -o <project dir> -s <product.json> --compiler iar <file>.syscfg" as a build step,
# and iarbuild -build deletes that step's output first. The real SysConfig for macOS runs on the MacBook
# (sysconfig_gen.py) and its output sits in $FRL_TI_ROOT/iar-gen/<sdk>/<project dir>; this copies it back.
out=""; while [ $# -gt 0 ]; do case "$1" in -o|--output) out=$2; shift;; esac; shift; done
TI="${FRL_TI_ROOT:-/opt/ti}"; stash="$TI/iar-gen${out#$TI}"
if [ -f "$stash/SysConfig_Generated_Files.ipcf" ]; then
  # fresh mtimes: ninja re-runs this step forever if its output is older than the .syscfg the project copies in
  cp -R "$stash/." "$out/" && (cd "$stash" && find . -type f -exec touch "$out/{}" \;) && echo "sysconfig shim: restored output from $stash"; exit 0
fi
echo "sysconfig shim: no pre-generated output in $stash (run sysconfig_gen.py on the Mac first)" >&2; exit 1
