#!/bin/bash
# Three ways, DriverLib on the LP-MSPM0C1104: clean build with the same glue_driverlib.c and common/app.c.
# Same steps as build.sh (which covers g3507, l1306), kept as its own file so the other boards' script stays as it was.
# Needs SysConfig 1.28.0 (1.26.3 is not installed on this Mac). Output: build-c1104/three_ways.out.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
STARTUP=startup_mspm0c110x_ticlang.c
O="$HERE/build-c1104"; LOG="$HERE/../logs/driverlib-c1104.log"
rm -rf "$O"; mkdir -p "$O/gen"
t0=$(python3 -c 'import time;print(time.time())')
{
  echo "== sysconfig"; "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$O/gen" "$HERE/c1104.syscfg"
  CF=(-I"$O/gen" -I"$HERE/../common" @"$O/gen/device.opt" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source"
      -O2 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -gdwarf-3)
  for f in "$HERE/../common/app.c" "$HERE/glue_driverlib.c" "$O/gen/ti_msp_dl_config.c" \
           "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP"; do
    echo "== cc $(basename "$f")"; "$CGT/bin/tiarmclang" "${CF[@]}" -c "$f" -o "$O/$(basename "${f%.c}").o"
  done
  echo "== link"
  "$CGT/bin/tiarmclang" -mcpu=cortex-m0plus -march=thumbv6m -mthumb -Wl,-u,_c_int00 "$O"/*.o \
    -Wl,-l"$O/gen/device.cmd.genlibs" -L"$SDK/source" "$O/gen/device_linker.cmd" \
    -Wl,-m,"$O/three_ways.map" -Wl,--rom_model -Wl,--warn_sections -L"$CGT/lib" -llibc.a -o "$O/three_ways.out"
} > "$LOG" 2>&1 || { tail -20 "$LOG"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
secs=$(python3 -c "print(round($t1-$t0,1))")
cp "$O/three_ways.map" "$HERE/../maps/driverlib-c1104.map"
echo "== clean build $secs s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CGT/bin/tiarmsize" "$O/three_ways.out" | tee -a "$LOG"
awk '/MEMORY CONFIGURATION/{f=1} f&&/^ *(FLASH|SRAM|BCR_CONFIG|BSL_CONFIG)/{print} /SEGMENT ALLOCATION MAP/{exit}' "$O/three_ways.map" | tee -a "$LOG"
