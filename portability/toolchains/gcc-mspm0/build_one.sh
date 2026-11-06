#!/bin/bash
# Build one MSPM0 SDK driverlib example with one toolchain in the shadow SDK; append one JSON line.
# Usage: build_one.sh <gcc|ticlang> <board> <example> <work dir> <shim dir> <data dir>
tc="$1"; board="$2"; ex="$3"; work="$4"; shim="$5"; data="$6"
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
ZSIZE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-size
d="$work/examples/nortos/$board/driverlib/$ex/$tc"
log="$data/logs/$tc-$board-$ex.log"; mkdir -p "$data/logs"
if [ ! -f "$d/makefile" ] && [ ! -f "$d/Makefile" ]; then
  python3 -I - "$tc" "$board" "$ex" >>"$data/$tc-$board.jsonl" <<'PY'
import json,sys
tc,b,e=sys.argv[1:]
print(json.dumps({"toolchain":tc,"board":b,"example":e,"built":False,"cause":"no_makefile_shipped","first_error":"no "+tc+" makefile in the example folder"}))
PY
  exit 0
fi
t0=$(python3 -c 'import time;print(time.time())')
( cd "$d" && make -j1 MSPM0_SDK_INSTALL_DIR="$work" SYSCONFIG_TOOL="$SYSCFG" \
   TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER="$shim" ) >"$log" 2>&1
rc=$?
t1=$(python3 -c 'import time;print(time.time())')
outf=$(ls "$d"/*.out 2>/dev/null | head -1)
size=""
if [ -n "$outf" ]; then
  if [ "$tc" = gcc ]; then size=$("$ZSIZE" "$outf" | tail -1 | awk '{print $1","$2","$3}')
  else size=$("$CGT/bin/tiarmsize" "$outf" 2>/dev/null | tail -1 | awk '{print $1","$2","$3}'); fi
fi
python3 -I - "$tc" "$board" "$ex" "$rc" "$t0" "$t1" "$size" "$log" >>"$data/$tc-$board.jsonl" <<'PY'
import json,sys,re
tc,b,e,rc,t0,t1,size,log=sys.argv[1:]
txt=open(log,errors="replace").read().splitlines()
warn=sum(1 for l in txt if "warning:" in l)
errs=[l for l in txt if re.search(r"error|fatal|undefined reference|No such file|\*\*\*|cannot find",l,re.I)]
s=[int(x) for x in size.split(",")] if size else None
print(json.dumps({"toolchain":tc,"board":b,"example":e,"built":rc=="0" and s is not None,"rc":int(rc),
 "secs":round(float(t1)-float(t0),1),"text":s[0] if s else None,"data":s[1] if s else None,"bss":s[2] if s else None,
 "warnings":warn,"first_error":(errs[0][:240] if errs else None)}))
PY
