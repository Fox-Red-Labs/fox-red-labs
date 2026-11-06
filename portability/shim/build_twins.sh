#!/bin/bash
# Build the ten TI Drivers twin cells in both directions with the shims in this folder. Builds only: no flash, no probe.
#   build_twins.sh m0_to_f3 <work dir> <results jsonl>
#   [MSPM0_BOARD=LP_MSPM0G3519] build_twins.sh f3_to_m0 <work dir> <results jsonl> <freertos.lib from ../crossbuild/build_mspm0_freertos.sh [board]>
# One JSON line per cell. Needs the MSPM0 SDK, the F3 SDK, SysConfig and TI Arm Clang under /Applications/ti. Never writes there.
DIR="$1"; W="$2"; OUT="$3"; LIB="$4"; mkdir -p "$W/logs"
HERE=$(cd "$(dirname "$0")" && pwd)
MS=/Applications/ti/mspm0_sdk_2_11_00_07; MB=${MSPM0_BOARD:-LP_MSPM0G3507}; ME=$MS/examples/rtos/$MB/drivers
F3=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS; FE=$F3/examples/rtos/LP_EM_CC2340R5/drivers
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=$(ls -d /Applications/ti/sysconfig_*/sysconfig_cli.sh | sort -V | tail -1)
# cell = MSPM0 example name : F3 example name
CELLS="adc_singlechannel:adcsinglechannel gpio_interrupt:gpiointerrupt i2c_controller:i2ccontroller i2c_target:i2ctarget i2c_tmp:i2ctmp spi_controller:spicontroller spi_peripheral:spiperipheral uart_callback:uart2callback uart_echo:uart2echo empty_freertos:empty"
m0app() { ls "$ME/$1"/*.c | grep -v -E 'main_freertos|ti_drivers_config|ti_msp_dl_config' | head -1; }
for c in $CELLS; do
  m=${c%%:*}; f=${c##*:}; d="$W/$DIR/$f"; rm -rf "$d"; mkdir -p "$W/$DIR"
  if [ "$DIR" = m0_to_f3 ]; then
    SHIM=$HERE/mspm0_on_f3; cp -R "$FE/$f" "$d"; fc=$(ls "$d"/*.c | head -1); mc=$(m0app $m); cp "$mc" "$fc"
    FL="-include $SHIM/fr_mspm0_on_f3.h -I$SHIM"
    # MSPM0 apps OR a pin-mux index into GPIO_setConfig; F3 needs none, so each one becomes 0.
    for n in $(grep -ohE '\bCONFIG_[A-Za-z0-9_]+_IOMUX\b' "$mc" | sort -u); do FL="$FL -D$n=0"; done
    # Board overlay: the MSPM0 app drives LED 0 but the F3 example's SysConfig has none.
    sc=$(ls "$d"/freertos/*.syscfg); if grep -q 'CONFIG_GPIO_LED_0' "$mc" && ! grep -q 'CONFIG_GPIO_LED_0' "$sc"; then cat "$SHIM/led0.syscfg" >> "$sc"; fi
    # Board overlay: the MSPM0 app prints through the UART display but the F3 example's SysConfig has no Display instance.
    if grep -q 'ti/display/DisplayUart.h' "$mc" && ! grep -q 'ti/display/Display' "$sc"; then cat "$SHIM/display.syscfg" >> "$sc"; fi
    log="$W/logs/$DIR-$f.log"
    ( cd "$d/freertos/ticlang" && CFLAGS="$FL" make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$F3 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT ) >"$log" 2>&1; rc=$?
    SRC="mspm0:$m"; DST="cc2340r5:$f"; APP="$mc"; TWIN="$FE/$f/$(basename $(ls $FE/$f/*.c | head -1))"
  else
    SHIM=$HERE/f3_on_mspm0; cp -R "$ME/$m" "$d"
    python3 "$HERE/../crossbuild/patch_mspm0_rtos_makefile.py" "$d/freertos/ticlang/makefile"
    mc=$(ls "$d"/*.c | grep -v -E 'main_freertos|ti_drivers_config|ti_msp_dl_config' | head -1); fc=$(ls "$FE/$f"/*.c | head -1)
    cp "$fc" "$mc"; for h in "$FE/$f"/*.h; do [ -f "$h" ] && cp "$h" "$d/"; done
    FL="-include $SHIM/fr_f3_on_mspm0.h -I$SHIM"
    # Board overlay: the MSPM0 example has no TI Drivers GPIO table, so GPIO calls on the LED become DriverLib calls.
    grep -q 'CONFIG_GPIO_LED_0' "$d/ti_drivers_config.h" || FL="$FL -DFR_NO_GPIO_DRIVER"
    log="$W/logs/$DIR-$m.log"
    ( cd "$d/freertos/ticlang" && CFLAGS="$FL" make MSPM0_SDK_INSTALL_DIR=$MS TICLANG_ARMCOMPILER=$CGT GCC_ARMCOMPILER= FREERTOS_LIB="$LIB" ) >"$log" 2>&1; rc=$?
    SRC="cc2340r5:$f"; DST="mspm0$(echo ${MB#LP_MSPM0} | tr A-Z a-z):$m"; APP="$fc"
  fi
  o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
  size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1","$2","$3}')
  errs=$(grep -E "error:|undefined symbol" "$log" | sed -E 's/^.*error: //' | sort | uniq -c | sort -rn | head -6 | sed 's/^ *//' | tr '\n' '|' | tr '"\\' "''")
  python3 - "$SRC" "$DST" "$DIR" "$rc" "$o" "$size" "$errs" "$(grep -cE 'error:|undefined symbol' "$log")" "$APP" "$mc" >>"$OUT" <<'PY'
import json,sys
src,dst,mode,rc,o,size,errs,n,app,used=sys.argv[1:]
s=[int(x) for x in size.split(",")] if size else None
print(json.dumps({"src":src,"dst":dst,"mode":"shim","dir":mode,"ok":rc=="0" and bool(o),
 "text":s[0] if s else None,"data":s[1] if s else None,"bss":s[2] if s else None,"n_errors":int(n),
 "errors":[e for e in errs.split("|") if e],"app":app,"built_app":used}))
PY
done
