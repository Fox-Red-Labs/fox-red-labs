#!/bin/bash
# Build the minimp3 bench for the LP-MSPM0G3519 (Cortex-M0+, soft-float) with arm-none-eabi-gcc and TI's MSPM0 DriverLib. Flashes nothing.
#   target-g3519/build.sh [opt: -O2|-O3|-Os]     output: $OUT/mp3.elf, .bin, .map, size.txt   (OUT default out/minimp3/g3519-<opt>)
# Env: MSPM0_SDK, XFLAGS (extra compiler flags, e.g. -DMP3_EMU for the emulator image), NSTACK (default 32768), OUT.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; P="$HERE/.."; T="$P/../../third-party/minimp3"
SDK=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}; OPT=${1:--O2}; NSTACK=${NSTACK:-32768}
O="${OUT:-$PWD/out/minimp3/g3519$OPT}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
python3 -I "$P/tools/embed_mp3.py" "$P/clips/stereo128.mp3" "$P/clips/mono64.mp3" clips.c
ARCH="-mthumb -mcpu=cortex-m0plus -msoft-float"
CF="$ARCH $OPT -g -ffunction-sections -fdata-sections -std=gnu99 -Wall -Wno-unused -D__MSPM0G3519__ -I$P/common -I$T -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include $XFLAGS"
for f in "$P/common/mp3run.c" "$P/common/mp3bench.c" "$HERE/main.c" clips.c "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g351x_gcc.c"; do
  arm-none-eabi-gcc $CF -c "$f" -o "$(basename "$f" .c).o" &
done; wait
arm-none-eabi-gcc $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$HERE/mspm0.ld" -Wl,--gc-sections -Wl,-Map=mp3.map \
  -Wl,--defsym=FRL_FLASH_LEN=0x80000 -Wl,--defsym=FRL_RAM_LEN=0x20000 -Wl,--defsym=FRL_STACK_LEN=$NSTACK -L"$SDK/source/ti/driverlib/lib/gcc/m0p/mspm0gx51x" \
  ./*.o -l:driverlib.a -lm -lc -lgcc -o mp3.elf
arm-none-eabi-objcopy -O binary mp3.elf mp3.bin
read t d b _ < <(arm-none-eabi-size mp3.elf | tail -1)
echo "g3519 $OPT flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) stack=$NSTACK flash_limit=524288 ram_limit=131072" | tee size.txt
