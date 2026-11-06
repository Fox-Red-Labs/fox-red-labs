#!/usr/bin/env bash
# Build the Zephyr tool tag for one or more boards. Touches no board.
#   tool-telemetry/tag-zephyr/build.sh                       nRF52840 DK, NUCLEO-WB55RG and LP-EM-CC2340R5
#   tool-telemetry/tag-zephyr/build.sh nrf52840dk/nrf52840   one board
# Zephyr 4.4.2 in ~/zephyrproject (ZP=...), the arm-zephyr-eabi compiler in ~/zephyr-toolchain (TC=...).
# Output: out/tool-telemetry/tag-zephyr/<board>/zephyr/zephyr.{elf,hex}, sizes in sizes.txt.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
OUT="$PWD/out/tool-telemetry/tag-zephyr"; mkdir -p "$OUT"
export ZEPHYR_BASE="$ZP/zephyr" ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
[ -f "$HERE/../ai/out/tt_model.c" ] || python3 "$HERE/../ai/train.py" > /dev/null
BOARDS=("$@"); [ ${#BOARDS[@]} -gt 0 ] || BOARDS=(nrf52840dk/nrf52840 nucleo_wb55rg lp_em_cc2340r5)
: > "$OUT/sizes.txt"
for b in "${BOARDS[@]}"; do
	d="$OUT/$(echo "$b" | tr '/' '_')"
	if ( cd "$ZP" && .venv/bin/west build -p always -b "$b" "$HERE" -d "$d" ) > "$d.log" 2>&1; then
		echo "$b: built $(grep -E 'FLASH:|RAM:' "$d.log" | tr -s ' ' | tr '\n' ' ')" | tee -a "$OUT/sizes.txt"
	else
		echo "$b: FAILED, see $d.log ($(grep -m1 -E 'error|Error' "$d.log" | cut -c1-160))" | tee -a "$OUT/sizes.txt"
	fi
done
