#!/bin/bash
# Compile the three MSP430 originals for the MSP430FR2433 with TI's MSP430 compiler 21.6.3.LTS (build only: no MSP430 board has been on the bench).
# Output goes to $OUT (default out/msp430-ports/msp430) with a map file; prints FRAM and RAM used.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-msp430_21.6.3.LTS; INC=/Applications/ti/ccs2101/ccs/ccs_base/msp430/include
OUT="${OUT:-$PWD/out/msp430-ports/msp430}"; mkdir -p "$OUT"
for pair in fram-persist:msp430fr243x_framwrite_persistent lpm3-wake:msp430fr243x_LPM3_01 uart-echo-lpm3:msp430fr243x_euscia0_uart_01; do
  P=${pair%%:*}; F=${pair##*:}
  "$CGT/bin/cl430" -vmspx --abi=eabi --data_model=small --code_model=large -O2 --define=__MSP430FR2433__ --printf_support=minimal -I"$INC" -I"$CGT/include" \
    "$HERE/$P/original/$F.c" -z -m"$OUT/$P.map" -i"$INC" -i"$CGT/lib" -llnk_msp430fr2433.cmd -llibc.a -o"$OUT/$P.out" > "$OUT/$P.log" 2>&1 \
    || { echo "$P: FAILED"; tail -5 "$OUT/$P.log"; continue; }
  echo "$P: $(grep -E '^ +(FRAM|RAM|FLASH) ' "$OUT/$P.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
done
