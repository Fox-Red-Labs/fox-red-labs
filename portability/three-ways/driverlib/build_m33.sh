#!/bin/bash
# Three ways, DriverLib on the MSPM33: clean build for LP-MSPM33C321A. Usage: ./build_m33.sh
# Runs SysConfig on m33c321a.syscfg (MSPM33 SDK product), compiles common/app.c + glue_driverlib_hsadc.c + the file
# SysConfig lists as referenced (the SDK's startup file) with TI Arm Clang -O2 (TI's example flags), links with the generated linker file.
# Output: build-m33c321a/three_ways.out (git-ignored); map and log copied to ../maps and ../logs.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=/Applications/ti/mspm33_sdk_1_04_00_00
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
STARTUP="$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"
O="$HERE/build-m33c321a"; LOG="$HERE/../logs/driverlib-m33c321a.log"
rm -rf "$O"; mkdir -p "$O/gen"
t0=$(python3 -c 'import time;print(time.time())')
{
  echo "== sysconfig"; "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$O/gen" "$HERE/m33c321a.syscfg"
  CF=(-I"$O/gen" -I"$HERE/../common" @"$O/gen/device.opt" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source"
      -O2 -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -Wall -gdwarf-3)
  for f in "$HERE/../common/app.c" "$HERE/glue_driverlib_hsadc.c" "$O/gen/ti_msp_dl_config.c" "$STARTUP"; do
    echo "== cc $(basename "$f")"; "$CGT/bin/tiarmclang" "${CF[@]}" -c "$f" -o "$O/$(basename "${f%.c}").o"
  done
  echo "== link"
  "$CGT/bin/tiarmclang" -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 "$O"/*.o \
    -Wl,-l"$O/gen/device.cmd.genlibs" -L"$SDK/source" "$O/gen/device_linker.cmd" \
    -Wl,-m,"$O/three_ways.map" -Wl,--rom_model -Wl,--warn_sections -L"$CGT/lib" -llibc.a -o "$O/three_ways.out"
} > "$LOG" 2>&1 || { tail -20 "$LOG"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
secs=$(python3 -c "print(round($t1-$t0,1))")
cp "$O/three_ways.map" "$HERE/../maps/driverlib-m33c321a.map"
echo "== clean build $secs s (load $(sysctl -n vm.loadavg))" | tee -a "$LOG"
"$CGT/bin/tiarmsize" "$O/three_ways.out" | tee -a "$LOG"
awk '/MEMORY CONFIGURATION/{f=1} f&&/^ *[A-Z_0-9]+ +[0-9a-f]{8}/{print} /SEGMENT ALLOCATION MAP/{exit}' "$O/three_ways.map" | tee -a "$LOG"
