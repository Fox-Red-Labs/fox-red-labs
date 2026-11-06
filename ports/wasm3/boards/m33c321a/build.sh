#!/bin/bash
# Build the wasm3 port for the LP-MSPM33C321A (Cortex-M33, FPU) with arm-none-eabi-gcc and TI's MSPM33 DriverLib.
#   boards/m33c321a/build.sh [heap-bytes] [wasm-stack-bytes]    output: boards/m33c321a/out/wasm3.elf, wasm3.out (same file, name DSLite likes), .bin, .map, size.txt
# Env: MSPM33_SDK, HEAPSZ, WSTACK, OPT (default -Os), XFLAGS, OUT. Nothing here flashes a board.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; P="$HERE/../.."; bash "$P/patches/apply.sh" "$P/out-src"; W="$P/out-src/source"
SDK=${MSPM33_SDK:-/Applications/ti/mspm33_sdk_1_04_00_00}; SU="$SDK/source/ti/devices/msp/m33/startup_system_files"
HEAPSZ=${1:-${HEAPSZ:-16384}}; WSTACK=${2:-${WSTACK:-4096}}; OPT=${OPT:--Os}; NSTACK=4096
O="${OUT:-$HERE/out}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
ARCH="-mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16"
CF="$ARCH $OPT -g -ffunction-sections -fdata-sections -std=gnu99 -w -D__MSPM33C321A__ -I$W -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include \
 -Dd_m3MaxNativeStack=$((NSTACK-512)) -Dd_m3FixedHeap=$HEAPSZ -Dd_m3CodePageAlignSize=1024 -Dd_m3VerboseErrorMessages=0 -Dd_m3MaxFunctionStackHeight=200 -Dd_m3MaxConstantTableSize=32 \
 -DFRL_WASM_STACK=$WSTACK -DFRL_FIB_N=${FIB_N:-20} -DFRL_BENCH_N=${BENCH_N:-20000} $XFLAGS"
SRCS=""; for f in $W/m3_*.c; do case $f in */m3_core.c|*/m3_api_*) ;; *) SRCS="$SRCS $f";; esac; done
for f in $SRCS "$P/common/m3_core_probe.c" "$P/common/frl_wasm.c" "$P/common/native.c" "$HERE/hal.c" "$HERE/main.c" "$SU/gcc/startup_mspm33c321x_gcc.c" "$SU/system_mspm33c321x.c"; do
  arm-none-eabi-gcc $CF -c "$f" -o "$(basename "$f" .c).o" &
done; wait
arm-none-eabi-gcc $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$HERE/mspm33.ld" -Wl,--gc-sections -Wl,-Map=wasm3.map,--cref \
  -Wl,--defsym=FRL_FLASH_LEN=0x100000 -Wl,--defsym=FRL_RAM_LEN=0x40000 -Wl,--defsym=FRL_STACK_LEN=0x1000 -L"$SDK/source/ti/driverlib/lib/gcc/m33/mspm33c321x" \
  ./*.o -l:driverlib.a -lm -lc -lgcc -o wasm3.elf
cp wasm3.elf wasm3.out; arm-none-eabi-objcopy -O binary wasm3.elf wasm3.bin
read t d b _ < <(arm-none-eabi-size wasm3.elf | tail -1)
echo "m33c321a flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) stack=4096 heap_in_bss=$HEAPSZ wasm_stack=$WSTACK flash_limit=1048576 ram_limit=262144" | tee size.txt
# BCR/BSL configuration areas must stay empty (mspm33c321a/AGENTS.md trap 1)
grep -E '^ +(BCR_CONFIG|BSL_CONFIG) ' wasm3.map || true
arm-none-eabi-nm wasm3.elf | grep -E ' (frl_result|frl_log|frl_log_len)$'
