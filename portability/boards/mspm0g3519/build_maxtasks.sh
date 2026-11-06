#!/bin/bash
# rtos_max_tasks for an MSPM0 Cortex-M0+ LaunchPad: FreeRTOS V11.2.0 from the MSPM0 SDK, TI's configMINIMAL_STACK_SIZE of 128 words,
# the largest heap_4 heap that links (single 128 kB SRAM region), minimal tasks made with xTaskCreate until it fails.
# Sources are mspm0g5187/freertos (edge.c, fr_common.c, FreeRTOSConfig.h), unchanged; only the linker file and the heap size differ.
#   ./build_maxtasks.sh        -> $OUT/maxtasks/edge-heap.out
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"; FR="$REPO/mspm0g5187/freertos"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
BOARD="${BOARD:-LP_MSPM0G3519}"; STARTUP="${STARTUP:-startup_mspm0g351x_ticlang.c}"
OUT="${OUT:-$PWD/out/speed-run/$(echo "$BOARD" | tr 'A-Z_' 'a-z-')}"; GEN="${GEN:-$OUT/gen/g80}"; O="$OUT/maxtasks"
K="$SDK/kernel/freertos/Source"; ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
INC="-I$FR -I$GEN -I$K/include -I$K/portable/TI_ARM_CLANG/ARM_CM0 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
rm -rf "$O"; mkdir -p "$O"; cd "$O"
cc() { local D="$1"; shift; for f in "$@"; do "$CC/bin/tiarmclang" -O2 $ARCH @"$GEN/device.opt" $INC -gdwarf-3 -Wall -DFR_MIN_STACK_WORDS=128 -DFR_CPU_HZ=80000000 -DEDGE_MAX=${EDGE_MAX:-1500} $D -c "$f" -o "$(basename "$f" .c).o"; done; }
sed -E 's/--stack_size=[0-9]+/--stack_size=256/; /origin = 0x20210000/d; s/SRAM_BANK0 +\(RWX\) : origin = 0x20200000, length = 0x00010000/SRAM (RWX) : origin = 0x20200000, length = 0x00020000/; s/SRAM_BANK0/SRAM/g' "$GEN/device_linker.cmd" > linker.cmd
link() { "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./linker.cmd -Wl,-m,edge-heap.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o edge-heap.out > link.log 2>&1; }
cc "-DFR_HEAP_BYTES=8" "$K/tasks.c" "$K/list.c" "$K/queue.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/port.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/portasm.c" "$FR/fr_common.c" "$FR/edge.c" "$GEN/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP"
cc "-DFR_HEAP_BYTES=8" "$K/portable/MemMang/heap_4.c"; link
left=$(python3 -c "import re;m=re.search(r'^\s+SRAM\s+\w+\s+\w+\s+\w+\s+(\w+)',open('edge-heap.map').read(),re.M);print(int(m.group(1),16))")
h=$(( (8 + left) / 8 * 8 ))
while true; do cc "-DFR_HEAP_BYTES=$h" "$K/portable/MemMang/heap_4.c"; link && break; h=$((h-8)); done
cc "-DFR_HEAP_BYTES=$((h+8))" "$K/portable/MemMang/heap_4.c"; if link; then echo "unexpected: $((h+8)) links"; exit 1; fi
echo "largest heap that links: $h bytes ($((h+8)) does not)" | tee search.txt
cc "-DFR_HEAP_BYTES=$h" "$FR/edge.c" "$K/portable/MemMang/heap_4.c" ; # edge.c depends on the heap size for its heap_total report
cc "-DFR_HEAP_BYTES=$h" "$K/tasks.c" "$K/list.c" "$K/queue.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/port.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/portasm.c"
link; grep -E '^ +SRAM ' edge-heap.map; echo "built $O/edge-heap.out"
