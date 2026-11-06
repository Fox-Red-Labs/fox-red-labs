#!/bin/bash
# Build the wasm3 port for the LP-MSPM0G3519 (default) or the LP-MSPM0G3507 (BOARD=g3507) with arm-none-eabi-gcc and TI's MSPM0 DriverLib.
#   boards/g3519/build.sh [heap-bytes] [stack-bytes]     output: boards/g3519/out-<board>/wasm3.elf, .bin, .map and size.txt
# Env: BOARD (g3519|g3507), MSPM0_SDK, HEAPSZ, WSTACK, OPT (default -Os), XFLAGS (extra compiler flags), OUT.
# Nothing here flashes a board.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; P="$HERE/../.."; bash "$P/patches/apply.sh" "$P/out-src"; W="$P/out-src/source"
BOARD=${BOARD:-g3519}; SDK=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}
HEAPSZ=${1:-${HEAPSZ:-16384}}; WSTACK=${2:-${WSTACK:-2048}}; OPT=${OPT:--Os}; NSTACK=${NSTACK:-8192}
if [ "$BOARD" = g3519 ]; then DEV=__MSPM0G3519__; LIBD=mspm0gx51x; START=startup_mspm0g351x_gcc.c; FL=0x80000; RAM=0x20000
else DEV=__MSPM0G3507__; LIBD=mspm0g1x0x_g3x0x; START=startup_mspm0g350x_gcc.c; FL=0x20000; RAM=0x8000; fi
O="${OUT:-$HERE/out-$BOARD}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
ARCH="-mthumb -mcpu=cortex-m0plus -msoft-float"
CF="$ARCH $OPT -g -ffunction-sections -fdata-sections -std=gnu99 -w -D$DEV -I$W -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include \
 -Dd_m3MaxNativeStack=$((NSTACK-512)) -Dd_m3FixedHeap=$HEAPSZ -Dd_m3CodePageAlignSize=1024 -Dd_m3HasFloat=0 -Dd_m3VerboseErrorMessages=0 -Dd_m3MaxFunctionStackHeight=200 -Dd_m3MaxConstantTableSize=32 \
 -DFRL_WASM_STACK=$WSTACK -DFRL_FIB_N=${FIB_N:-20} -DFRL_BENCH_N=${BENCH_N:-20000} $XFLAGS"
SRCS=""; for f in $W/m3_*.c; do case $f in */m3_core.c|*/m3_api_*) ;; *) SRCS="$SRCS $f";; esac; done
for f in $SRCS "$P/common/m3_core_probe.c" "$P/common/frl_wasm.c" "$P/common/native.c" "$HERE/hal.c" "$HERE/main.c" "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/$START"; do
  arm-none-eabi-gcc $CF -c "$f" -o "$(basename "$f" .c).o" &
done; wait
arm-none-eabi-gcc $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$HERE/mspm0.ld" -Wl,--gc-sections -Wl,-Map=wasm3.map \
  -Wl,--defsym=FRL_FLASH_LEN=$FL -Wl,--defsym=FRL_RAM_LEN=$RAM -Wl,--defsym=FRL_STACK_LEN=$NSTACK -L"$SDK/source/ti/driverlib/lib/gcc/m0p/$LIBD" \
  ./*.o -l:driverlib.a -lm -lc -lgcc -o wasm3.elf
arm-none-eabi-objcopy -O binary wasm3.elf wasm3.bin
read t d b _ < <(arm-none-eabi-size wasm3.elf | tail -1)
echo "$BOARD flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) stack=$NSTACK heap_in_bss=$HEAPSZ wasm_stack=$WSTACK flash_limit=$((FL)) ram_limit=$((RAM))" | tee size.txt
