#!/bin/bash
# Build the LC3 bench for a Cortex-M0+ (soft float, as on the LP-MSPM0G3519) with arm-none-eabi-gcc, 10 frames per configuration, for the emulator. Flashes nothing.
#   target-g3519/build.sh [opt: -O2|-Os]     output $OUT/lc3.elf, .bin, .map, size.txt   (OUT default out/bench-day/lc3/g3519<opt>)
# Env: MSPM0_SDK (for the startup file), NSTACK (default 16384), OUT, FM (default -ffp-contract=off; set -ffast-math for upstream's flag).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; P="$HERE/.."; L="$P/../../third-party/liblc3"
SDK=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}; OPT=${1:--O2}; NSTACK=${NSTACK:-16384}
O="${OUT:-$PWD/out/bench-day/lc3/g3519$OPT}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
ARCH="-mthumb -mcpu=cortex-m0plus -msoft-float"
CF="$ARCH $OPT ${FM:--ffp-contract=off} -g -ffunction-sections -fdata-sections -std=gnu99 -Wall -Wno-unused -D__MSPM0G3519__ -DLC3_BENCH_FRAMES=10 -I$P/common -I$L/include -I$L/src -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include"
for s in attdet bits bwdet energy lc3 ltpf mdct plc sns spec tables tns; do arm-none-eabi-gcc $CF -c "$L/src/$s.c" -o "$s.o" & done
for f in "$P/common/lc3run.c" "$HERE/main.c" "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g351x_gcc.c"; do arm-none-eabi-gcc $CF -c "$f" -o "$(basename "$f" .c).o" & done; wait
arm-none-eabi-gcc $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$HERE/mspm0.ld" -Wl,--gc-sections -Wl,-Map=lc3.map \
  -Wl,--defsym=FRL_FLASH_LEN=0x80000 -Wl,--defsym=FRL_RAM_LEN=0x20000 -Wl,--defsym=FRL_STACK_LEN=$NSTACK ./*.o -lm -lc -lgcc -o lc3.elf
arm-none-eabi-objcopy -O binary lc3.elf lc3.bin
read t d b _ < <(arm-none-eabi-size lc3.elf | tail -1)
echo "g3519 $OPT flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) stack=$NSTACK flash_limit=524288 ram_limit=131072" | tee size.txt
