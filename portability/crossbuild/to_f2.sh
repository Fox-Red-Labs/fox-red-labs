#!/bin/bash
# Cross-build into the LP-CC1354P10-1 (F2 SDK, Cortex-M33): TI Drivers example app source from the
# CC2340R5 (F3 SDK) and from the MSPM0G3507 (MSPM0 SDK), each built inside its F2 twin project.
# MSPM0 sources also get a "shim" mode with portability/shim/mspm0_on_f3 (F2 shares F3's names).
# Usage: [F2_SDK=<sdk dir>] [SKIP_MSPM0=1] to_f2.sh <work dir> <results jsonl>
W="$1"; OUT="$2"; mkdir -p "$W/logs"
HERE=$(cd "$(dirname "$0")" && pwd); SHIM=$(cd "$HERE/../shim/mspm0_on_f3" && pwd)
F2=${F2_SDK:-/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16}; E2=$F2/examples/rtos/LP_EM_CC1354P10_1/drivers
F3E=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/rtos/LP_EM_CC2340R5/drivers
ME=/Applications/ti/mspm0_sdk_2_11_00_07/examples/rtos/LP_MSPM0G3507/drivers
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}
# F2 example : CC2340R5 twin : MSPM0 twin ("-" when none)
ROWS="empty:empty:empty_freertos gpiointerrupt:gpiointerrupt:gpio_interrupt adcsinglechannel:adcsinglechannel:adc_singlechannel i2ctmp:i2ctmp:i2c_tmp spicontroller:spicontroller:spi_controller spiperipheral:spiperipheral:spi_peripheral uart2callback:uart2callback:uart_callback uart2echo:uart2echo:uart_echo"
build() { # $1 F2 example, $2 label, $3 source file or "", $4 extra CFLAGS, $5 src name
  d="$W/$2/$1"; rm -rf "$d"; mkdir -p "$W/$2"; cp -R "$E2/$1" "$d"
  fc=$(ls "$d"/*.c | head -1); [ -n "$3" ] && cp "$3" "$fc"
  log="$W/logs/$2-$1.log"
  ( cd "$d/freertos/ticlang" && CFLAGS="$4" make SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=$F2 SYSCONFIG_TOOL=$SYSCFG TICLANG_ARMCOMPILER=$CGT ) >"$log" 2>&1
  rc=$?; o=$(ls "$d"/freertos/ticlang/*.out 2>/dev/null | head -1)
  size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1}')
  err=$(grep -m1 -E "error:|undefined symbol" "$log" | sed -E 's/^.*error: //' | cut -c1-200 | tr '"\\' "''")
  echo "{\"src\":\"$5\",\"dst\":\"cc1354p10:$1\",\"mode\":\"$2\",\"ok\":$([ $rc = 0 ] && [ -n "$o" ] && echo true || echo false),\"text\":${size:-null},\"n_errors\":$(grep -cE 'error:|undefined symbol' "$log"),\"first_error\":\"$err\"}" >>"$OUT"
}
for r in $ROWS; do
  f2=${r%%:*}; rest=${r#*:}; f3=${rest%%:*}; m=${rest#*:}
  build "$f2" baseline "" "" "cc1354p10:$f2"
  [ -n "$SKIP_MSPM0" ] && { build "$f2" from_cc2340r5 "$(ls $F3E/$f3/*.c | head -1)" "" "cc2340r5:$f3"; continue; }
  build "$f2" from_cc2340r5 "$(ls $F3E/$f3/*.c | head -1)" "" "cc2340r5:$f3"
  mc=$(ls "$ME/$m"/*.c | grep -v -E 'main_freertos|ti_drivers_config|ti_msp_dl_config')
  build "$f2" from_mspm0 "$mc" "" "mspm0g3507:$m"
  sf="-include $SHIM/fr_mspm0_on_f3.h -I$SHIM"
  for n in $(grep -ohE '\bCONFIG_[A-Za-z0-9_]+_IOMUX\b' "$mc" | sort -u); do sf="$sf -D$n=0"; done
  build "$f2" from_mspm0_shim "$mc" "$sf" "mspm0g3507:$m"
done
