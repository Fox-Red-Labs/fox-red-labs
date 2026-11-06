#!/bin/bash
# Read-only identify of the TMDS62LEVM over its on-board XDS110 (J8). Writes nothing to the board.
# Needs board power on J17. Reads: JTAG user id, boot-mode register, then prints them.
# Usage: am62l-evm/tools/identify.sh [outdir]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
DS=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
RUN="${RUN:?set RUN to the folder with AM62L_TMDS62LEVM_nogel.ccxml and images/}"
# Same no-GEL connection file as load_sram.sh (same RUN and targetdb). The stock CCStudio board file's
# A53 GEL initializes the DDR on connect (PLL, PD_DDR_PowerUp, DDR_Init), which a read-only identify must not do.
CFG="$RUN/AM62L_TMDS62LEVM_nogel.ccxml"
[ -f "$CFG" ] || { echo "no $CFG; run am62l-evm/tools/load_sram.sh --list-cores once to generate it"; exit 3; }
OUT="${1:-$RUN/identify}"
mkdir -p "$OUT"
echo "== probe =="; "$DS" identifyProbe --config="$CFG" 2>&1 | tail -5
rd() { # name addr
  "$DS" memory --config="$CFG" --range="$2,1" --size=32 --output="$OUT/$1.bin" 2>&1 | tail -2
  [ -s "$OUT/$1.bin" ] && echo "$1 @ $2 = 0x$(xxd -e -g4 "$OUT/$1.bin" | awk '{print $2}')"
}
echo "== registers =="
rd jtag_user_id 0x43000018   # WKUP_CTRL_MMR_CFG0_JTAG_USER_ID; AM62L32 expect 0x391A7, AM62L31 0x39187
rd bootmode     0x43010030   # boot-mode register (SD boot reads 0x243)
