#!/bin/bash
# Build MicroPython v1.29.0 (ports/zephyr) for the LP-EM-CC2755P10 with TI's Zephyr fork (simplelink-zephyr v3.7.0-ti-1.00.01_ga).
# Variables: MP MicroPython checkout, ZP fork west workspace, TC toolchain, VENV python venv, OUT build folder.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MP="${MP:-$HOME/micropython}"; OUT="${OUT:-$PWD/out/build-mp-cc2755p10}"
ZP="${ZP:-$HOME/zephyr-ti/ws}"; TC="${TC:-$HOME/zephyr-toolchain}"; VENV="${VENV:-$ZP/../.venv}"
. "$VENV/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
cd "$ZP/zephyr"
west build -p always -b lp_em_cc2755p10 "$MP/ports/zephyr" -d "$OUT" -- -DCONF_FILE="$HERE/prj_cc2755p10.conf"
