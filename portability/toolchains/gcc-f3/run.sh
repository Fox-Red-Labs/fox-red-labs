#!/bin/bash
# Rerunnable: GCC vs TI Arm Clang builds of the SimpleLink Low Power F3 SDK examples (nortos and FreeRTOS), unchanged, in a scratch copy.
# Usage: run.sh [boards...]  (default LP_EM_CC2340R5 LP_EM_CC2755P10)   env: WORK, JOBS, ONLY (substring of the example id)
here="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
WORK=${WORK:-/private/tmp/gcc-f3-work}; JOBS=${JOBS:-4}
ZROOT=$HOME/zephyr-toolchain/arm-zephyr-eabi
SHIM=$WORK/gcc-shim
boards=("$@"); [ ${#boards[@]} -eq 0 ] && boards=(LP_EM_CC2340R5 LP_EM_CC2755P10)
mkdir -p "$WORK/examples" "$SHIM/bin"
for x in source kernel .metadata tools tfm_s imports.mak; do [ -e "$WORK/$x" ] || ln -s "$SDK/$x" "$WORK/$x"; done
# Name shim: the SDK makefiles call arm-none-eabi-*; the Zephyr SDK GCC has the prefix arm-zephyr-eabi-.
for t in gcc size objdump objcopy ar nm; do
  printf '#!/bin/bash\nexec %s/bin/arm-zephyr-eabi-%s "$@"\n' "$ZROOT" "$t" > "$SHIM/bin/arm-none-eabi-$t"; chmod +x "$SHIM/bin/arm-none-eabi-$t"
done
[ -e "$SHIM/arm-none-eabi" ] || ln -s "$ZROOT/arm-zephyr-eabi" "$SHIM/arm-none-eabi"
mkdir -p "$here/data/logs"
for b in "${boards[@]}"; do
  rm -rf "$WORK/examples/rtos/$b" "$WORK/examples/nortos/$b"
  for k in rtos nortos; do mkdir -p "$WORK/examples/$k"; cp -R "$SDK/examples/$k/$b" "$WORK/examples/$k/$b"; done
  rm -f "$here/data/gcc-$b.jsonl" "$here/data/ticlang-$b.jsonl"; rm -f "$here"/data/logs/*-"$b"_*.log
  # every example folder that ships a gcc/ or a ticlang/ makefile folder
  ( cd "$WORK/examples"; find rtos/"$b" nortos/"$b" -type d \( -name gcc -o -name ticlang \) | sed -E 's#/(gcc|ticlang)$##' | sort -u ) | \
    sed -E "s#^(rtos|nortos)/$b/#\1/#" | { [ -n "$ONLY" ] && grep -- "$ONLY" || cat; } > "$WORK/list-$b.txt"
  for tc in gcc ticlang; do
    xargs -P "$JOBS" -I{} "$here/build_one.sh" $tc $b {} "$WORK" "$SHIM" "$here/data" < "$WORK/list-$b.txt"
  done
done
