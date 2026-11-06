#!/bin/bash
# Cross-build: MSPM0 SDK TI Drivers example app source, built inside its CC2340R5 (F3 SDK) twin project.
# The F3 project supplies board config (SysConfig), FreeRTOS and linker file; only the app .c is swapped.
# Usage: m0_to_f3.sh <work dir> <results jsonl>
# Writes one JSON line per pair. Never writes into /Applications/ti.
W="$1"; OUT="$2"; mkdir -p "$W/logs"
M=/Applications/ti/mspm0_sdk_2_11_00_07/examples/rtos/LP_MSPM0G3507/drivers
F3=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
FE=$F3/examples/rtos/LP_EM_CC2340R5/drivers
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
SHIM=$(cd "$(dirname "$0")/../shim/mspm0_on_f3" && pwd)
PAIRS="adc_singlechannel:adcsinglechannel gpio_interrupt:gpiointerrupt i2c_controller:i2ccontroller i2c_target:i2ctarget i2c_tmp:i2ctmp spi_controller:spicontroller spi_peripheral:spiperipheral uart_callback:uart2callback uart_echo:uart2echo empty_freertos:empty"
for p in $PAIRS; do
  m=${p%%:*}; f=${p##*:}
  for mode in ${MODES:-baseline swapped shim}; do
    d="$W/$mode/$f"; rm -rf "$d"; mkdir -p "$W/$mode"; cp -R "$FE/$f" "$d"
    fc=$(ls "$d"/*.c | head -1)
    mc=$(ls "$M/$m"/*.c | grep -v -E 'main_freertos|ti_drivers_config|ti_msp_dl_config')
    [ $mode != baseline ] && cp "$mc" "$fc"
    SHIMFLAGS=""
    if [ $mode = shim ]; then
      SHIMFLAGS="-include $SHIM/fr_mspm0_on_f3.h -I$SHIM"
      # MSPM0 apps OR a pin-mux index into GPIO_setConfig flags; F3 needs none, so each one becomes 0.
      for n in $(grep -ohE '\bCONFIG_[A-Za-z0-9_]+_IOMUX\b' "$mc" | sort -u); do SHIMFLAGS="$SHIMFLAGS -D$n=0"; done
    fi
    log="$W/logs/$mode-$f.log"
    ( cd "$d/freertos/ticlang" && CFLAGS="$SHIMFLAGS" make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$F3 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT ) >"$log" 2>&1
    rc=$?; o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1","$2","$3}')
    errs=$(grep -E "error:" "$log" | sed -E 's/^.*error: //' | sort | uniq -c | sort -rn | head -6 | sed 's/^ *//' | tr '\n' '|' | tr '"\\' "''")
    python3 - "$m" "$f" "$mode" "$rc" "$o" "$size" "$errs" "$(grep -c 'error:' "$log")" >>"$OUT" <<'PY'
import json,sys
m,f,mode,rc,o,size,errs,n=sys.argv[1:]
s=[int(x) for x in size.split(",")] if size else None
print(json.dumps({"src":"mspm0:"+m,"dst":"cc2340r5:"+f,"mode":mode,"ok":rc=="0" and bool(o),
 "text":s[0] if s else None,"data":s[1] if s else None,"bss":s[2] if s else None,"n_errors":int(n),
 "errors":[e for e in errs.split("|") if e]}))
PY
  done
done
