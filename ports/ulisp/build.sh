#!/bin/bash
# Build uLisp for the LP-MSPM0G5187 (default), LP-MSPM0G3519 (BOARD=g3519) or LP-MSPM0G3507 (BOARD=g3507) with arm-none-eabi-g++ and TI's MSPM0 DriverLib.
#   ports/ulisp/build.sh        output: ports/ulisp/build-<board>/ulisp.elf, .bin, .map, size.txt
# Env: BOARD, MSPM0_SDK, AUTOEXEC (Lisp text to run at start-up), OUT (output folder), WORKSPACE (objects of 8 bytes; default per board), OPT (default -Os). Nothing here flashes a board.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BOARD=${BOARD:-g5187}; SDK=${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}; OPT=${OPT:--Os}
case $BOARD in
  g5187) DEV=__MSPM0G5187__; LIBD=mspm0g518x; START=startup_mspm0g518x_gcc.c; FL=0x20000; RAM=0x8000; WS=${WORKSPACE:-2800}; BD=-DFRL_G5187;;
  g3507) DEV=__MSPM0G3507__; LIBD=mspm0g1x0x_g3x0x; START=startup_mspm0g350x_gcc.c; FL=0x20000; RAM=0x8000; WS=${WORKSPACE:-2800}; BD=-DFRL_G3507;;
  g3519) DEV=__MSPM0G3519__; LIBD=mspm0gx51x; START=startup_mspm0g351x_gcc.c; FL=0x80000; RAM=0x20000; WS=${WORKSPACE:-9000}; BD=-DFRL_G3519;;
  *) echo "BOARD must be g5187, g3507 or g3519"; exit 1;;
esac
O="${OUT:-$HERE/build-$BOARD}"; rm -rf "$O"; mkdir -p "$O"; cd "$O"
python3 "$HERE/patches/apply.py" "$O/src" > /dev/null
if [ -n "$AUTOEXEC" ]; then   # text the REPL reads first, as if typed (used for the LED check, which resets the chip)
  python3 -c 'import sys,json; print("#define FRL_AUTOEXEC " + json.dumps(sys.argv[1] + "\r"))' "$AUTOEXEC" > "$O/autoexec.h"
fi
ARCH="-mthumb -mcpu=cortex-m0plus -msoft-float"
INC="-I$HERE/shim -I$O -D$DEV $BD -I$SDK/source -I$SDK/source/third_party/CMSIS/Core/Include"
[ -n "$AUTOEXEC" ] && BD="$BD -DFRL_LED_OUT"
CC="$ARCH $OPT -g -ffunction-sections -fdata-sections -w $INC"
CXX="$CC -std=gnu++17 -fno-exceptions -fno-rtti -DFRL_MSPM0 -DFRL_WORKSPACE=$WS"
arm-none-eabi-g++ $CXX -c src/ulisp-arm.cpp -o ulisp.o &
arm-none-eabi-g++ $CXX -c "$HERE/shim/arduino_shim.cpp" -o shim.o &
arm-none-eabi-g++ $CXX -c "$HERE/boards/main.cpp" -o main.o &
arm-none-eabi-gcc $CC -std=gnu99 -c "$HERE/boards/hal.c" -o hal.o &
arm-none-eabi-gcc $CC -std=gnu99 -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/$START" -o startup.o &
wait
STACKMIN=${STACKMIN:-6144}
arm-none-eabi-g++ $ARCH --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,-T,"$HERE/boards/mspm0.ld" -Wl,--gc-sections -Wl,-Map=ulisp.map \
  -Wl,--defsym=FRL_FLASH_LEN=$FL -Wl,--defsym=FRL_RAM_LEN=$RAM -Wl,--defsym=FRL_STACK_LEN=$STACKMIN -L"$SDK/source/ti/driverlib/lib/gcc/m0p/$LIBD" \
  ./ulisp.o ./shim.o ./main.o ./hal.o ./startup.o -l:driverlib.a -lm -lc -lgcc -o ulisp.elf
arm-none-eabi-objcopy -O binary ulisp.elf ulisp.bin
read t d b _ < <(arm-none-eabi-size ulisp.elf | tail -1)
echo "$BOARD flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) workspace_objects=$WS workspace_bytes=$((WS*8)) stack_bytes=$((RAM-d-b)) flash_limit=$((FL)) ram_limit=$((RAM))" | tee size.txt
