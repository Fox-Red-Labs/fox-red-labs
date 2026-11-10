#!/bin/bash
# Build uLisp for the LP-MSPM0L1306 (64 kB flash, 4 kB RAM) with arm-none-eabi-g++ and DriverLib. BUILD ONLY: never touches a probe.
#   ports/ulisp/boards/mspm0l1306/build.sh     output: $OUT/ulisp.elf, .bin, .map, size.txt (OUT default ports/ulisp/build-l1306)
# Env: FLASHLEN (default 0x10000; a larger value measures an image that does not fit), WORKSPACE (objects of 8 bytes, default 40), STACKMIN (bytes the linker must leave for the stack, default 1024), OPT (default -Os), MSPM0_SDK.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; UL="$(cd "$HERE/../.." && pwd)"
SDK=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}; OPT=${OPT:--Os}; WS=${WORKSPACE:-40}; STACKMIN=${STACKMIN:-1024}
O="${OUT:-$UL/build-l1306}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
python3 "$UL/patches/apply.py" "$O/src" > /dev/null
ARCH="-mthumb -mcpu=cortex-m0plus -msoft-float"
INC="-I$UL/shim -D__MSPM0L1306__ -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include"
CC="$ARCH $OPT -g -ffunction-sections -fdata-sections -w $INC"
CXX="$CC -std=gnu++17 -fno-exceptions -fno-rtti -DFRL_MSPM0 -DFRL_WORKSPACE=$WS"
arm-none-eabi-g++ $CXX -c src/ulisp-arm.cpp -o ulisp.o &
arm-none-eabi-g++ $CXX -c "$UL/shim/arduino_shim.cpp" -o shim.o &
arm-none-eabi-g++ $CXX -c "$UL/boards/main.cpp" -o main.o &
arm-none-eabi-gcc $CC -std=gnu99 -c "$HERE/hal.c" -o hal.o &
arm-none-eabi-gcc $CC -std=gnu99 -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0l130x_gcc.c" -o startup.o &
wait
set +e
arm-none-eabi-g++ $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$UL/boards/mspm0.ld" -Wl,--gc-sections -Wl,-Map=ulisp.map \
  -Wl,--defsym=FRL_FLASH_LEN=${FLASHLEN:-0x10000} -Wl,--defsym=FRL_RAM_LEN=0x1000 -Wl,--defsym=FRL_STACK_LEN=$STACKMIN -L"$SDK/source/ti/driverlib/lib/gcc/m0p/mspm0l11xx_l13xx" \
  ./ulisp.o ./shim.o ./main.o ./hal.o ./startup.o -l:driverlib.a -lm -lc -lgcc -o ulisp.elf 2> link.log
rc=$?
if [ $rc != 0 ]; then echo "LINK FAILED"; grep -E "overflow|FRL_STACK|undefined|region" link.log | head -5; fi
# size from the object files if the link fails (sum of the same input sections)
if [ -f ulisp.elf ]; then
read t d b _ < <(arm-none-eabi-size ulisp.elf | tail -1)
echo "l1306 flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) workspace_objects=$WS workspace_bytes=$((WS*8)) stack_bytes=$((0x1000-d-b)) flash_limit=$((${FLASHLEN:-0x10000})) ram_limit=4096" | tee size.txt
fi
exit 0
