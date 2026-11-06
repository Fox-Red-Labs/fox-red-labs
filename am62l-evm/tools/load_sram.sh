#!/bin/bash
# E199: load the DDR-less Zephyr ELF into on-chip SRAM over JTAG (XDS110 on J8) and run it. The DDR is never initialized.
# Usage: am62l-evm/tools/load_sram.sh [--dry-run] [--listen] [--elf FILE] [--run-dir DIR] [--serial S] [--timeout SEC] [--core N] [--list-cores] [--peek ADDR,WORDS ...]
#   --dry-run   print every command, touch nothing (no probe, no serial port, no files outside the run dir's generated targetdb)
#   --listen    first start a timestamped capture of the four FTDI ports (J7) at 115200 8N1, logs in the run dir
# Why a generated connection file: the stock board file runs AM62L_DDRSS/AM62L_SK_EVM.gel on connect (PLL, DDR power-up, DDR_Init).
# This script uses a copy of the board file with empty GEL properties, so nothing but the JTAG access happens at connect.
#   --list-cores  connect (no GEL, reads only) and print the core order, then stop; run it once to confirm which -n is CortexA53_0
# Never run against a board whose NOR or eMMC you care about without the backup step in the arrival plan; this script writes only SRAM.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
DS=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
CCSDB=/Applications/ti/ccs2101/ccs/ccs_base/common/targetdb
RUN="${RUN:?set RUN to the folder with AM62L_TMDS62LEVM_nogel.ccxml and images/}"
ELF="$RUN/images/build-sram-all/zephyr.elf"
SERIAL="${SERIAL:?set SERIAL to the probe serial}"
TMO=30
CORE=0; LISTCORES=0
DRY=0; LISTEN=0
while [ $# -gt 0 ]; do case "$1" in
  --dry-run) DRY=1;; --listen) LISTEN=1;;
  --elf) ELF="$2"; shift;; --run-dir) RUN="$2"; shift;; --serial) SERIAL="$2"; shift;; --timeout) TMO="$2"; shift;; --core) CORE="$2"; shift;; --list-cores) LISTCORES=1;; --peek) PEEK="$2"; shift;;
  *) echo "unknown option $1"; exit 2;; esac; shift; done
PORTS="/dev/cu.usbserial-072518103640 /dev/cu.usbserial-072518103641 /dev/cu.usbserial-072518103642 /dev/cu.usbserial-072518103643"
TDB="$RUN/targetdb"; CCXML="$RUN/AM62L_TMDS62LEVM_nogel.ccxml"; STAMP=$(date +%Y%m%d-%H%M%S)
run() { if [ "$DRY" = 1 ]; then echo "DRY: $*"; else "$@"; fi; }

# 1. generated targetdb (symlinks to the install, plus our board file) and ccxml
echo "== connection file =="
run mkdir -p "$TDB/boards"
for d in Modules boarddat connections cpus devices drivers options routers setup_parser.xsd setup_parser_cache.xsd; do
  run ln -sfn "$CCSDB/$d" "$TDB/$d"; done
run cp "$HERE/AM62L_TMDS62LEVM_nogel_board.xml" "$TDB/boards/AM62L_TMDS62LEVM_nogel.xml"
if [ "$DRY" = 1 ]; then echo "DRY: sed @TARGETDB@=$TDB @SERIAL@=$SERIAL < ccxml.template > $CCXML"
else sed -e "s#@TARGETDB@#$TDB#g" -e "s/@SERIAL@/$SERIAL/g" "$HERE/AM62L_TMDS62LEVM.ccxml.template" > "$CCXML"; fi
grep -q 'GEL File" Value=""\|Value="" id="GEL File"' "$HERE/AM62L_TMDS62LEVM_nogel_board.xml" || { echo "board file has a GEL set; refusing (DDR must not be initialized)"; exit 3; }
[ "$DRY" = 1 ] || [ -f "$ELF" ] || { echo "no ELF at $ELF"; exit 3; }

if [ "$LISTCORES" = 1 ]; then
  run "$DS" flash --config="$CCXML" --list-cores; exit $?
fi
# 1b. --peek ADDR,WORDS [ADDR,WORDS ...]: read-only 32-bit memory reads after a run (E199 diagnosis). Reads only.
if [ -n "${PEEK:-}" ]; then
  for r in $PEEK; do
    o="$RUN/peek-$(echo $r | tr ',' '_').bin"
    "$DS" memory --config="$CCXML" --core="$CORE" --range="$r" --size=32 --output="$o" >/dev/null 2>&1
    [ -s "$o" ] && echo "$r: $(xxd -e -g4 "$o" | awk '{$1="";print}' | tr -s ' ' | cut -c2-)" || echo "$r: read failed"
  done
  exit 0
fi
# 2. optional capture
if [ "$LISTEN" = 1 ]; then
  echo "== capture (all four UARTs, 115200 8N1) =="
  for i in 0 1 2 3; do
    p=$(echo $PORTS | cut -d' ' -f$((i+1)))
    log="$RUN/e199-$STAMP-port$i.log"
    if [ "$DRY" = 1 ]; then echo "DRY: python3 -I $HERE/capture.py $p 115200 $log $RUN/e199-$STAMP-port$i.raw &"
    else
      [ -e "$p" ] || { echo "missing $p"; continue; }
      python3 -I "$HERE/capture.py" "$p" 115200 "$log" "$RUN/e199-$STAMP-port$i.raw" &
      echo $! > "$RUN/e199-$STAMP-port$i.pid"
    fi
  done
  run sleep 2
fi

# 3. load and run. DSLite connects to core 0 (A53_0 in the generated board file), halts it, writes the ELF segments to SRAM,
#    sets PC to the ELF entry point and leaves the core running. No GEL runs. Entry 0x707F08BC, one LOAD at 0x707F0000.
echo "== load =="
run "$DS" load --config="$CCXML" --core="$CORE" --timeout="$TMO" --log="$RUN/e199-$STAMP-dslite.log" "$ELF"
RC=$?
echo "DSLite exit $RC at $(date +%H:%M:%S)"
if [ "$LISTEN" = 1 ] && [ "$DRY" = 0 ]; then
  echo "Capture is running; watch $RUN/e199-$STAMP-port0.log. Stop with: kill \$(cat $RUN/e199-$STAMP-port*.pid)"
fi
exit $RC
