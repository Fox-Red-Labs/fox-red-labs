#!/bin/bash
# Flash one DSP image to the LP-MSPM0G5187 (probe MG020001) with DSLite, let it start itself (no debugger reset: a reset
# during an NPU run can wedge the NPU), halt, read the result block, decode it. Holds the probe lock for flash plus read.
#   ./run.sh <model> <npu|cpu>      e.g. ./run.sh fir64 npu
#   ./run.sh cbase                  the plain C baseline
# Raw words go to results/<name>.txt. Prints one decoded line.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
if [ -z "${FRL_LOCK_HELD:-}" ]; then exec python3 "$REPO/tools/frl/lock.py" exec --serial "${PROBE:?set PROBE to the probe serial}" --op "npu-dsp $*" -- "$0" "$@"; fi
if [ "$1" = cbase ]; then NAME=cbase; IMG=$PWD/out/npu-dsp/build/cbase/cbase.out; SYM=cb; W=18
else NAME=$1-$2; IMG=$PWD/out/npu-dsp/build/$NAME/dsp.out; SYM=dsp; W=30; fi
mkdir -p "$HERE/results"
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg
NM=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
A=$($NM "$IMG" | awk -v s="$SYM" '$3==s {print $1}')
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c "$REPO/mspm0g5187/MSPM0G5187_MG020001.ccxml" -e -f -v -u "$IMG" > "$PWD/out/npu-dsp/$NAME.flash.txt" 2>&1
grep -E "Success|Error|Fail" "$PWD/out/npu-dsp/$NAME.flash.txt" | tail -1
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial ${PROBE:?set PROBE to the probe serial}" -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" -c init -c "sleep 6000" -c halt -c "mdw 0x$A $W" -c shutdown 2>&1 | grep -E "^0x" > "$HERE/results/$NAME.txt" || true
python3 - "$NAME" "$HERE/results/$NAME.txt" <<'PY'
import sys
name, f = sys.argv[1:]
w = [int(x, 16) for l in open(f) for x in l.split(":")[1].split()]
if not w: print(name, "NO DATA"); sys.exit(1)
clk = w[1]
if name == "cbase":
    lab = ["fir_1ch", "fir_4ch", "bank_8ch"]
    print(f"{name} magic {w[0]:08x} clk {clk} " + " | ".join(f"{lab[i]}: mism {w[2+i]} med {w[6+i]} ({w[6+i]/clk*1e3:.3f} ms) min {w[10+i]} max {w[14+i]}" for i in range(3)))
else:
    cy = w[9:29]
    print(f"{name} magic {w[0]:08x} clk {clk} nout {w[3]} mismatches {w[4]} maxdiff {w[5]} min {w[6]} ({w[6]/clk*1e3:.4f} ms) med {w[7]} ({w[7]/clk*1e3:.4f} ms) max {w[8]} ({w[8]/clk*1e3:.4f} ms) spin {w[29]} overhead {w[2]}")
PY
