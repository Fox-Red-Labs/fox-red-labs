#!/bin/bash
# Rerunnable: GCC vs TI Arm Clang builds of the MSPM0 nortos driverlib examples, unchanged, in a scratch copy.
# Usage: run.sh [boards...]   (default: LP_MSPM0G3507 LP_MSPM0L1306)   env: WORK, JOBS, ONLY (one example name)
here="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
WORK=${WORK:-/private/tmp/gcc-mspm0-work}; JOBS=${JOBS:-4}
ZROOT=$HOME/zephyr-toolchain/arm-zephyr-eabi
SHIM=$WORK/gcc-shim
boards=("$@"); [ ${#boards[@]} -eq 0 ] && boards=(LP_MSPM0G3507 LP_MSPM0L1306)
mkdir -p "$WORK/examples/nortos" "$SHIM/bin"
for x in source kernel .metadata tools imports.mak; do [ -e "$WORK/$x" ] || ln -s "$SDK/$x" "$WORK/$x"; done
# imports.mak is read through the symlink; every path in it is overridden on the make command line.
# Name shim: the SDK makefiles call arm-none-eabi-*; the Zephyr SDK GCC has the prefix arm-zephyr-eabi-.
for t in gcc size objdump; do
  printf '#!/bin/bash\nexec %s/bin/arm-zephyr-eabi-%s "$@"\n' "$ZROOT" "$t" > "$SHIM/bin/arm-none-eabi-$t"; chmod +x "$SHIM/bin/arm-none-eabi-$t"
done
[ -e "$SHIM/arm-none-eabi" ] || ln -s "$ZROOT/arm-zephyr-eabi" "$SHIM/arm-none-eabi"
for b in "${boards[@]}"; do
  rm -rf "$WORK/examples/nortos/$b"; mkdir -p "$WORK/examples/nortos/$b"
  cp -R "$SDK/examples/nortos/$b/driverlib" "$WORK/examples/nortos/$b/driverlib"
  rm -f "$here/data/gcc-$b.jsonl" "$here/data/ticlang-$b.jsonl"; rm -f "$here"/data/logs/*-$b-*.log
  for tc in gcc ticlang; do
    ls "$WORK/examples/nortos/$b/driverlib" | { [ -n "$ONLY" ] && grep -x "$ONLY" || cat; } | \
      xargs -P "$JOBS" -I{} "$here/build_one.sh" $tc $b {} "$WORK" "$SHIM" "$here/data"
  done
done
