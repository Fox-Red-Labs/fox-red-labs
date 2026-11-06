#!/bin/bash
# Flash one example, let it run <wait> s, halt, print the PC with its symbol, run extra OpenOCD commands, resume.
#   ./probe.sh <example> <wait-s> [ocd cmd ...]
set -o pipefail
EX="$1"; W="$2"; shift 2; HERE="$(cd "$(dirname "$0")" && pwd)"; R="$HERE/../.."; NAME="${EX//\//_}"
OUT="$HERE/build/$NAME/$NAME.out"; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
mkdir -p "$HERE/runs"
# A failed load must stop the probe: halting now would read the PC of whatever was flashed before.
if ! DOUT=$(/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite load --config="$R/mspm0l1306/MSPM0L1306_ML130001.ccxml" -f "$OUT" 2>&1); then
  printf '%s\n' "$DOUT" | grep -E "Success|rror|Failed" | sed "s|^|$EX dslite: |" || true
  echo "$EX: FLASH FAILED, not probing"; exit 5
fi
printf '%s\n' "$DOUT" | grep -E "Success|rror|Failed" | sed "s|^|$EX dslite: |" || true
sleep "$W"
PC=$("$R/mspm0l1306/tools/ocd.sh" halt "reg pc" "$@" resume 2>&1 | tee "$HERE/runs/$NAME.probe" | { grep -E "^pc " || true; } | awk '{print $3}')
[ -n "$PC" ] || { echo "$EX: no PC read back (see $HERE/runs/$NAME.probe)"; exit 6; }
SYM=$("$CC/bin/tiarmnm" -n "$OUT" | awk -v pc=$((PC)) '{a=strtonum("0x"$1)} a<=pc {s=$3} END {print s}' 2>/dev/null || true)
[ -z "$SYM" ] && SYM=$(python3 - "$OUT" "$PC" <<'PY'
import subprocess,sys
cc="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm"
pc=int(sys.argv[2],16); best=("?",0)
for l in subprocess.run([cc,"-n",sys.argv[1]],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if len(p)==3 and p[1] in "TtWw":
        a=int(p[0],16)
        if a<=pc and a>=best[1]: best=(p[2],a)
print(best[0])
PY
)
echo "$EX: PC $PC in $SYM; $(grep -E '^[A-Z0-9_]+ 0x|^0x' "$HERE/runs/$NAME.probe" | tr '\n' ' ')"
