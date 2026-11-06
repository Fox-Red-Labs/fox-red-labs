#!/bin/bash
# Everything for the NUCLEO-U083RC that builds without the board, into out/nucleo-u083rc/.
#   ./prebuild.sh                  build all (refuses while the Mac is busy; prints install steps and stops if a tool is missing)
#   ./prebuild.sh --check          only report what is installed and what is missing
#   ./prebuild.sh --fetch-sources  clone ST's STM32CubeU0 (public GitHub, no login) and fetch Zephyr's hal_stm32 module
# Builds: Zephyr blinky; ST's GPIO_IOToggle as shipped and with a changed delay (the blink proof); ST's U0 versions of the
# five examples ported to the LP-MSPM0G3507; ST's AES and RNG examples (the crypto pair); CoreMark (unmodified EEMBC files)
# with GCC 12.2, ST's GCC 14.3 and ST's Clang at ST example clocks; all four room-occupancy models; the sweep build pass.
# Nothing is flashed. Shared functions live with the G071RB (layout rule: they move to a top-level folder on merge).
#
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../nucleo-g071rb/bench/st-common/st_build.sh"

BOARD_ID=nucleo-u083rc
CUBE_NAME=STM32CubeU0; CUBE_TAG=v1.3.0; CUBE_REF=c87491e6bd00f64fbaca68471ec5cf4563c870d8   # main branch head read 2026-10-07
CUBE_BOARD=NUCLEO-U083RC; CUBE_REPO_SIZE="82.7 MB"
CUBE_SUBMODULES="Drivers/STM32U0xx_HAL_Driver Drivers/CMSIS/Device/ST/STM32U0xx Drivers/BSP/STM32U0xx_Nucleo Drivers/BSP/Components/Common Middlewares/ST/threadx Middlewares/ST/cmsis_rtos_threadx Middlewares/ST/filex Middlewares/ST/levelx Middlewares/ST/usbx Middlewares/ST/STM32_TouchSensing_Library"
BENCH_HAL_H='"stm32u0xx_hal.h"'
ZEPHYR_BOARD=nucleo_u083rc
TOP_MHZ=56                                   # data sheet maximum; built only if an ST example for this board runs there
OUT="$PWD/out/$BOARD_ID"

case "${1:-}" in
  --check) st_find_tools; st_report_missing; exit $? ;;
  --fetch-sources) st_load_guard; st_find_tools; st_fetch_sources; exit $? ;;
esac

st_load_guard
st_find_tools
if ! st_report_missing; then echo; echo "Stopped: install or fetch the missing pieces above, then run ./prebuild.sh again."; exit 2; fi
mkdir -p "$OUT"
exec > >(tee "$OUT/prebuild.log") 2>&1
echo "prebuild $BOARD_ID $(date '+%Y-%m-%d %H:%M:%S'); CubeCLT GCC: $CLT_GCC; STARM-Clang: ${STARM:-none}; GCC 12.2: $GCC12"
P="$CUBE/Projects/$CUBE_BOARD"

echo "== 1. Zephyr blinky (Zephyr 4.4.2, board $ZEPHYR_BOARD)"
st_zephyr_blinky "$ZEPHYR_BOARD" "$OUT"

echo "== 2. ST's blink: GPIO_IOToggle as shipped (100 ms) and changed (250 ms), ST's GCC, -Os"
st_build_example "$P/Examples/GPIO/GPIO_IOToggle" "$OUT/blink-st" "$CLT_GCC" -Os
sed 's/HAL_Delay(100);/HAL_Delay(250);/' "$P/Examples/GPIO/GPIO_IOToggle/Src/main.c" > "$OUT/blink-changed-main.c"
grep -q 'HAL_Delay(250);' "$OUT/blink-changed-main.c" || echo "WARN: the 100 ms delay was not found; the changed blink is not changed"
ST_MAIN_OVERRIDE="$OUT/blink-changed-main.c" st_build_example "$P/Examples/GPIO/GPIO_IOToggle" "$OUT/blink-changed" "$CLT_GCC" -Os

echo "== 3. ST's U0 versions of the five ported examples, and the crypto pair (ST's GCC, -Os)"
for ex in Examples/GPIO/GPIO_IOToggle Examples_LL/USART/USART_Communication_Tx_VCP_Init Examples/GPIO/GPIO_EXTI \
          Examples/ADC/ADC_MultiChannelSingleConversion Examples_LL/TIM/TIM_PWMOutput_Init \
          Examples/CRYP/CRYP_AESModes Examples/RNG/RNG_Config Examples_LL/RNG/RNG_GenerateRandomNumbers; do
  printf '%s: ' "$ex"; st_build_example "$P/$ex" "$OUT/originals/$(basename "$ex")" "$CLT_GCC" -Os || echo "FAILED (see $OUT/originals/$(basename "$ex")/build.log)"
done

echo "== 4. CoreMark at ST example clocks"
CLK_EX="$P/Examples/GPIO/GPIO_IOToggle"                     # 48 MHz per its README
TOP_EX=$(grep -l -E "(HCLK|SystemCoreClock)[^.]*$TOP_MHZ ?MHz" "$P"/Examples/*/*/README.md 2>/dev/null | while read -r r; do
  d=$(dirname "$r"); grep -q HAL_RCC_OscConfig "$d/Src/main.c" 2>/dev/null && echo "$d"; done | head -1)
echo "top-clock example: ${TOP_EX:-none found for $TOP_MHZ MHz; that clock is not built}" | tee "$OUT/clock-examples.txt"
for c in "$CLK_EX" ${TOP_EX:+"$TOP_EX"}; do
  m=$(st_clock_cfg "$c/Src/main.c" "$OUT/.probe.c" | tr -d ' ' | tr 'A-Z' 'a-z'); m=${m:-unknown}
  st_build_bench coremark "gcc-bare-o3-unroll-$m" "$GCC12" "-O3 -funroll-loops" "$c" "$OUT"
  st_build_bench coremark "gcc-bare-o2-$m" "$GCC12" "-O2" "$c" "$OUT"
  st_build_bench coremark "gcc-bare-os-$m" "$GCC12" "-Os" "$c" "$OUT"
  st_build_bench coremark "stgcc14-bare-o3-$m" "$CLT_GCC" "-O3" "$c" "$OUT"
  st_build_bench coremark "stgcc14-bare-o3-unroll-$m" "$CLT_GCC" "-O3 -funroll-loops" "$c" "$OUT"
  if [ -n "$STARM" ]; then   # ST's hybrid mode (starm-clang.md); a plain starm-clang call cannot link ST's GNU linker scripts
    st_build_bench coremark "starmclang-bare-o3-$m" "$HERE/bench/starm-hybrid.sh" "-O3" "$c" "$OUT"
    st_build_bench coremark "starmclang-bare-omax-$m" "$HERE/bench/starm-hybrid.sh" "--config=Omax.cfg" "$c" "$OUT"; fi
done
st_build_bench coremark "gcc-bare-o3-unroll-reset" "$GCC12" "-O3 -funroll-loops" reset "$OUT" "" 1000

echo "== 5. Room-occupancy models, plain C, at the 48 MHz ST clock (40 kB RAM: all four should link)"
for mdl in occ_cnn_500 occ_cnn_2k occ_cnn_8k occ_cnn_30k; do
  st_build_bench occ "$mdl-gcc12-o3" "$GCC12" "-O3" "$CLK_EX" "$OUT" "$mdl"
  st_build_bench occ "$mdl-gcc12-os" "$GCC12" "-Os" "$CLK_EX" "$OUT" "$mdl"
  st_build_bench occ "$mdl-stgcc14-o3" "$CLT_GCC" "-O3" "$CLK_EX" "$OUT" "$mdl"
done

echo "== 6. Example sweep, build pass (every STM32CubeIDE project for $CUBE_BOARD, ST's GCC, -Os; ROT and option-byte projects skipped)"
st_sweep_build "$CLT_GCC" "$OUT"

rm -f -- "$OUT/.probe.c"
du -sh "$OUT"; echo "done $(date '+%H:%M:%S'); log: $OUT/prebuild.log"
