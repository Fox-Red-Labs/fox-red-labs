#!/bin/bash
# FreeRTOS images for the LP-MSPM0C1104 (24 MHz, 16 kB flash, 1 kB RAM), TI Arm Clang 5.1.1 and the kernel in MSPM0 SDK
# 2.11.00.07 (V11.2.0, read only). BUILD ONLY. Output: out/mspm0c1104/freertos/<name>/
#   ./build.sh <name> <app.c> [MIN_STACK_WORDS] [MSP_STACK_BYTES] [extra -D flags...]
#   ./build.sh blink blink.c 32 128         one task and idle
# Prints flash and RAM from the linker map, or the linker's "will not fit" message.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
GEN=$PWD/out/mspm0c1104/gen/g24; K="$SDK/kernel/freertos/Source"
NAME=$1 APP=$2 MIN=${3:-32} MSP=${4:-128}; shift 4 || shift $#
O="$PWD/out/mspm0c1104/freertos/$NAME"; rm -rf "$O"; mkdir -p "$O"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
INC="-I$HERE -I$GEN -I$K/include -I$K/portable/TI_ARM_CLANG/ARM_CM0 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
for f in "$K/tasks.c" "$K/list.c" "$K/queue.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/port.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/portasm.c" \
         "$HERE/fr_common.c" "$HERE/$APP" "$GEN/ti_msp_dl_config.c" \
         "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0c110x_ticlang.c"; do
  (cd "$O" && "$CC/bin/tiarmclang" -O2 $ARCH @"$GEN/device.opt" $INC -gdwarf-3 -Wall -fstack-usage -DFR_MIN_STACK_WORDS=$MIN "$@" -c "$f" -o "$(basename "$f" .c).o")
done
sed "s/--stack_size=64/--stack_size=$MSP/" "$GEN/device_linker.cmd" > "$O/device_linker.cmd"
if (cd "$O" && "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
      -Wl,-m,"$NAME.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$NAME.out" > link.log 2>&1); then
  "$CC/bin/tiarmsize" "$O/$NAME.out" | tail -1 | awk -v n="$NAME" '{printf "%s: text %d data %d bss %d  (flash = text+data = %d, RAM = data+bss = %d of 1024)\n", n, $1, $2, $3, $1+$2, $2+$3}'
  "$CC/bin/tiarmnm" -S -n --defined-only "$O/$NAME.out" | awk '$1 ~ /^2000/ && NF == 4' > "$O/ram-symbols.txt"
else
  echo "$NAME: does not link: $(grep -m2 -E 'will not fit|overflow|error' "$O/link.log" | tr '\n' ' ')"
fi
