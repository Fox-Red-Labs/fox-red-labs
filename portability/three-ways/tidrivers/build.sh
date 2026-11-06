#!/bin/bash
# Three ways, TI Drivers: clean build for one board. Usage: ./build.sh g3507|l1306 [freertos|nortos]
# freertos (default): what TI ships for MSPM0 TI Drivers. The FreeRTOS kernel library is built first,
#   timed on its own, with ../../crossbuild/build_mspm0_freertos.sh (the SDK's makefile cannot build it).
# nortos: the same glue on TI's prebuilt no-RTOS driver porting layer (kernel/nortos/lib); no example ships this way.
# Compiler flags are TI's adc_singlechannel makefile's. Output: build-<board>-<kernel>/ (git-ignored).
set -euo pipefail
B="${1:?board: g3507 or l1306}"; K="${2:-freertos}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
case "$B" in
  g3507) BOARD=LP_MSPM0G3507; DEV=__MSPM0G3507__; LIBS=mspm0g1x0x_g3x0x
         EX=$SDK/examples/rtos/LP_MSPM0G3507/drivers/adc_singlechannel/freertos/ticlang
         STARTUP=$EX/startup_mspm0g350x_ticlang.c; CMD=$EX/mspm0g3507.cmd
         NORTOS_STARTUP=$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c ;;
  l1306) BOARD=LP_MSPM0L1306; DEV=__MSPM0L1306__; LIBS=mspm0l11xx_l13xx
         # No TI Drivers example exists for this board: TI's startup and linker files from the SDK's device folder.
         STARTUP=$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0l130x_ticlang.c
         CMD=$SDK/source/ti/devices/msp/m0p/linker_files/ticlang/mspm0l1306.cmd; NORTOS_STARTUP=$STARTUP ;;
  *) echo "unknown board $B"; exit 2 ;;
esac
O="$HERE/build-$B-$K"; LOG="$HERE/../logs/tidrivers-$B-$K.log"
rm -rf "$O"; mkdir -p "$O"
: > "$LOG"
KINC=(); KLIB=()
if [ "$K" = freertos ]; then
  k0=$(python3 -c 'import time;print(time.time())')
  "$HERE/../../crossbuild/build_mspm0_freertos.sh" "$O/kernel" "$BOARD" >> "$LOG" 2>&1
  k1=$(python3 -c 'import time;print(time.time())')
  echo "== kernel library $(python3 -c "print(round($k1-$k0,1))") s" | tee -a "$LOG"
  KINC=(-I"$SDK/kernel/freertos/Source/include" -I"$SDK/kernel/freertos/Source/portable/TI_ARM_CLANG/ARM_CM0"
        -I"$SDK/source/ti/posix/ticlang" -I"$SDK/kernel/freertos/builds/$BOARD/release")
  KLIB=("-Wl,-l$O/kernel/freertos.lib")
else
  KLIB=("-Wl,-l$SDK/kernel/nortos/lib/ticlang/m0p/nortos_$LIBS.a")
  STARTUP=$NORTOS_STARTUP
fi
t0=$(python3 -c 'import time;print(time.time())')
{
  CF=(-I"$HERE/$B" -I"$HERE/../common" -D"$DEV" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source" ${KINC[@]+"${KINC[@]}"}
      -O2 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -gdwarf-3)
  for f in "$HERE/../common/app.c" "$HERE/glue_tidrivers.c" "$HERE/main_$K.c" "$HERE/$B/ti_drivers_config.c" "$HERE/$B/ti_msp_dl_config.c" "$STARTUP"; do
    echo "== cc $(basename "$f")"; "$CGT/bin/tiarmclang" "${CF[@]}" -c "$f" -o "$O/$(basename "${f%.c}").o"
  done
  echo "== link"
  "$CGT/bin/tiarmclang" -mcpu=cortex-m0plus -march=thumbv6m -mthumb -Wl,-u,_c_int00 "$O"/*.o \
    "-Wl,-l$SDK/source/ti/drivers/lib/ticlang/m0p/drivers_$LIBS.a" ${KLIB[@]+"${KLIB[@]}"} \
    "-Wl,-l$SDK/source/ti/driverlib/lib/ticlang/m0p/$LIBS/driverlib.a" -L"$SDK/source" "$CMD" \
    -Wl,-m,"$O/three_ways.map" -Wl,--rom_model -Wl,--warn_sections -L"$CGT/lib" -llibc.a -o "$O/three_ways.out"
} >> "$LOG" 2>&1 || { tail -25 "$LOG"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
cp "$O/three_ways.map" "$HERE/../maps/tidrivers-$B-$K.map"
echo "== clean build (app, kernel library not counted) $(python3 -c "print(round($t1-$t0,1))") s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CGT/bin/tiarmsize" "$O/three_ways.out" | tee -a "$LOG"
awk '/MEMORY CONFIGURATION/{f=1} f&&/^ *(FLASH|SRAM|BCR_CONFIG|BSL_CONFIG)/{print} /SEGMENT ALLOCATION MAP/{exit}' "$O/three_ways.map" | tee -a "$LOG"
