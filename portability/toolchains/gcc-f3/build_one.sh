#!/bin/bash
# Build one F3 SDK example folder with one toolchain in the scratch SDK; append one JSON line.
# Usage: build_one.sh <gcc|ticlang> <board> <example id> <work dir> <shim dir> <data dir>
# example id = <kernel>/<path to the folder that holds gcc/ and ticlang/>, e.g. rtos/drivers/empty/freertos or nortos/drivers/empty
tc="$1"; board="$2"; ex="$3"; work="$4"; shim="$5"; data="$6"
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
ZSIZE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-size
kernel=${ex%%/*}; rel=${ex#*/}
d="$work/examples/$kernel/$board/$rel/$tc"
id="$board/$ex"; tag=$(echo "$id" | tr '/' '_')
log="$data/logs/$tc-$tag.log"; mkdir -p "$data/logs"
rec() { python3 -I -c '
import json,sys,re
tc,b,e,rc,t0,t1,size,log,cause=sys.argv[1:]
try: txt=open(log,errors="replace").read().splitlines()
except OSError: txt=[]
warn=sum(1 for l in txt if "warning:" in l)
errs=[l for l in txt if re.search(r"error|fatal|undefined reference|No such file|\*\*\*|cannot find|not found",l,re.I)]
s=[int(x) for x in size.split(",")] if size else None
print(json.dumps({"toolchain":tc,"board":b,"example":e,"built":rc=="0" and s is not None,"rc":int(rc),
 "secs":round(float(t1)-float(t0),1),"text":s[0] if s else None,"data":s[1] if s else None,"bss":s[2] if s else None,
 "warnings":warn,"cause":cause or None,"first_error":(errs[0].strip()[:240] if errs else None)}))' "$@"; }
if [ ! -f "$d/makefile" ] && [ ! -f "$d/Makefile" ]; then
  echo "no $tc makefile in the example folder" > "$log"
  rec "$tc" "$board" "$ex" 1 0 0 "" "$log" no_makefile_shipped >>"$data/$tc-$board.jsonl"; exit 0
fi
t0=$(python3 -c 'import time;print(time.time())')
( cd "$d" && make -j1 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$work" SYSCONFIG_TOOL="$SYSCFG" \
   TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER="$shim" ) >"$log" 2>&1
rc=$?
t1=$(python3 -c 'import time;print(time.time())')
outf=$(ls "$d"/*.out 2>/dev/null | head -1)
size=""
if [ -n "$outf" ]; then
  if [ "$tc" = gcc ]; then size=$("$ZSIZE" "$outf" | tail -1 | awk '{print $1","$2","$3}')
  else size=$("$CGT/bin/tiarmsize" "$outf" 2>/dev/null | tail -1 | awk '{print $1","$2","$3}'); fi
fi
rec "$tc" "$board" "$ex" "$rc" "$t0" "$t1" "$size" "$log" "" >>"$data/$tc-$board.jsonl"
# keep disk small: drop everything the build produced once the size is recorded
rm -rf "$d"/*.obj "$d"/*.out "$d"/*.hex "$d"/*.map "$d"/ti_* "$d"/*.o 2>/dev/null
exit 0
