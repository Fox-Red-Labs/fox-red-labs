#!/bin/bash
# One-command blink on the NUCLEO-U083RC with ST's command-line tools (STM32CubeCLT 1.22.0), no IDE.
#   ./blink.sh [delay_ms]        default 250 (ST's example ships with 100)
# Builds ST's GPIO_IOToggle example from STM32CubeU0 with the delay changed, using ST's GCC 14.3 from CubeCLT,
# flashes it with STM32_Programmer_CLI over the onboard STLINK-V2EC, verifies and resets. LD4 (PA5) blinks.
# Needs: STM32CubeCLT in /opt/ST, and ST's examples fetched by ./prebuild.sh --fetch-sources.
# PROBE_SERIAL picks the board when more than one ST-LINK is plugged in.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../nucleo-g071rb/bench/st-common/st_build.sh"
CUBE_NAME=STM32CubeU0; CUBE_BOARD=NUCLEO-U083RC; BENCH_HAL_H='"stm32u0xx_hal.h"'
DELAY="${1:-250}"; OUT="${OUT:-$PWD/out/nucleo-u083rc}"
st_find_tools
[ -n "$CLT_GCC" ] || { echo "STM32CubeCLT not found in /opt/ST. Install it first (README, step 1)."; exit 2; }
[ -n "$CUBE" ] || { echo "ST's examples are missing. Run ./prebuild.sh --fetch-sources first."; exit 2; }
CLI=$(ls /opt/ST/STM32CubeCLT*/STM32CubeProgrammer/bin/STM32_Programmer_CLI | tail -1)
EX="$CUBE/Projects/$CUBE_BOARD/Examples/GPIO/GPIO_IOToggle"
mkdir -p "$OUT"
sed "s/HAL_Delay(100);/HAL_Delay($DELAY);/" "$EX/Src/main.c" > "$OUT/blink-main.c"
grep -q "HAL_Delay($DELAY);" "$OUT/blink-main.c" || { echo "Could not change the delay in ST's example."; exit 3; }
t0=$(date +%s)
ST_MAIN_OVERRIDE="$OUT/blink-main.c" st_build_example "$EX" "$OUT/blink" "$CLT_GCC" -Os
t1=$(date +%s)
"$CLI" -c port=SWD ${PROBE_SERIAL:+sn=$PROBE_SERIAL} -w "$OUT/blink/app.elf" -v -rst > "$OUT/blink-flash.log" 2>&1 \
  || { tail -5 "$OUT/blink-flash.log"; echo "Flash failed."; exit 4; }
t2=$(date +%s)
grep -E "Download verified successfully" "$OUT/blink-flash.log" || true
echo "Built in $((t1 - t0)) s, flashed in $((t2 - t1)) s. LD4 should now toggle every $DELAY ms."
