#!/bin/bash
# Three ways, DriverLib: clean build for one board. Usage: ./build.sh g3507|l1306|g5187
# Runs SysConfig on <board>.syscfg, compiles common/app.c + glue_driverlib.c + TI's startup file
# with TI Arm Clang -O2 (TI's example flags), links with TI's generated linker file.
# Output: build-<board>/three_ways.out (git-ignored); map and log copied to ../maps and ../logs.
set -euo pipefail
B="${1:?board: g3507, l1306, g5187 or g3519}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=${FRL_MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}
CGT=${FRL_TICLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
SYSCFG=${FRL_SYSCONFIG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}
[ -x "$SYSCFG" ] || SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
case "$B" in
  g3507) STARTUP=startup_mspm0g350x_ticlang.c ;;
  l1306) STARTUP=startup_mspm0l130x_ticlang.c ;;
  g5187) STARTUP=startup_mspm0g518x_ticlang.c ;;
  g3519) STARTUP=startup_mspm0g351x_ticlang.c ;;
  *) echo "unknown board $B"; exit 2 ;;
esac
O="$HERE/build-$B"; LOG="$HERE/../logs/driverlib-$B.log"
rm -rf "$O"; mkdir -p "$O/gen"
t0=$(python3 -c 'import time;print(time.time())')
{
  echo "== sysconfig"; "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$O/gen" "$HERE/$B.syscfg"
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
cp "$O/three_ways.map" "$HERE/../maps/driverlib-$B.map"
echo "== clean build $secs s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CGT/bin/tiarmsize" "$O/three_ways.out" | tee -a "$LOG"
awk '/MEMORY CONFIGURATION/{f=1} f&&/^ *(FLASH|SRAM|BCR_CONFIG|BSL_CONFIG)/{print} /SEGMENT ALLOCATION MAP/{exit}' "$O/three_ways.map" | tee -a "$LOG"
