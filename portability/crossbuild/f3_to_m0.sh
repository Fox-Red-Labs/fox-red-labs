#!/bin/bash
# Cross-build: F3 SDK (CC2340R5) TI Drivers example app source, built inside its MSPM0 SDK (G3507) twin.
# The MSPM0 project supplies board config, FreeRTOS and linker file; only the app .c is swapped.
# Needs the FreeRTOS library from build_mspm0_freertos.sh. Usage: f3_to_m0.sh <work dir> <results jsonl> <freertos.lib>
W="$1"; OUT="$2"; LIB="$3"; mkdir -p "$W/logs"
HERE=$(cd "$(dirname "$0")" && pwd); SHIM=$(cd "$HERE/../shim/f3_on_mspm0" && pwd)
MS=/Applications/ti/mspm0_sdk_2_11_00_07; ME=$MS/examples/rtos/LP_MSPM0G3507/drivers
FE=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/rtos/LP_EM_CC2340R5/drivers
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
PAIRS="adcsinglechannel:adc_singlechannel gpiointerrupt:gpio_interrupt i2ccontroller:i2c_controller i2ctarget:i2c_target i2ctmp:i2c_tmp spicontroller:spi_controller spiperipheral:spi_peripheral uart2callback:uart_callback uart2echo:uart_echo empty:empty_freertos"
for p in $PAIRS; do
  f=${p%%:*}; m=${p##*:}
  for mode in ${MODES:-baseline swapped shim}; do
    d="$W/$mode/$m"; rm -rf "$d"; mkdir -p "$W/$mode"; cp -R "$ME/$m" "$d"
    python3 "$HERE/patch_mspm0_rtos_makefile.py" "$d/freertos/ticlang/makefile"
    mc=$(ls "$d"/*.c | grep -v -E 'main_freertos|ti_drivers_config|ti_msp_dl_config' | head -1)
    fc=$(ls "$FE/$f"/*.c | head -1)
    [ $mode != baseline ] && cp "$fc" "$mc"
    SHIMFLAGS=""; [ $mode = shim ] && SHIMFLAGS="-include $SHIM/fr_f3_on_mspm0.h -I$SHIM"
    log="$W/logs/$mode-$m.log"
    ( cd "$d/freertos/ticlang" && CFLAGS="$SHIMFLAGS" make MSPM0_SDK_INSTALL_DIR=$MS TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER= FREERTOS_LIB="$LIB" ) >"$log" 2>&1
    rc=$?; o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1","$2","$3}')
    errs=$(grep -E "error:|undefined symbol" "$log" | sed -E 's/^.*error: //' | sort | uniq -c | sort -rn | head -6 | sed 's/^ *//' | tr '\n' '|' | tr '"\\' "''")
    python3 - "$f" "$m" "$mode" "$rc" "$o" "$size" "$errs" "$(grep -cE 'error:|undefined symbol' "$log")" >>"$OUT" <<'PY'
import json,sys
f,m,mode,rc,o,size,errs,n=sys.argv[1:]
s=[int(x) for x in size.split(",")] if size else None
print(json.dumps({"src":"cc2340r5:"+f,"dst":"mspm0g3507:"+m,"mode":mode,"ok":rc=="0" and bool(o),
 "text":s[0] if s else None,"data":s[1] if s else None,"bss":s[2] if s else None,"n_errors":int(n),
 "errors":[e for e in errs.split("|") if e]}))
PY
  done
done
