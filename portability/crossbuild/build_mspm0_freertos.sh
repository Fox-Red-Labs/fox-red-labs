#!/bin/bash
# Build the MSPM0 SDK's FreeRTOS kernel library from its CCStudio .projectspec file list.
# The SDK's kernel makefile points at a ticlang/ folder that has no makefile, so a terminal
# or agent build cannot make this library as shipped. Usage: build_mspm0_freertos.sh <out dir> [board]
O="$1"; B=${2:-LP_MSPM0G3507}; mkdir -p "$O/obj"
M=/Applications/ti/mspm0_sdk_2_11_00_07
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
PS=$(ls $M/kernel/freertos/builds/$B/release/ticlang/*.projectspec)
DEV=$(grep -oE 'deviceId="[^"]+"' "$PS" | head -1 | sed -E 's/.*="([^"]+)"/\1/')
FLAGS=$(python3 -c "import re,sys;t=open(sys.argv[1]).read();m=re.search(r'compilerBuildOptions=\"([^\"]*)\"',t,re.S);print(' '.join(m.group(1).split()))" "$PS" | sed "s#\${COM_TI_MSPM0_SDK_INSTALL_DIR}#$M#g; s#\${PROJECT_ROOT}#$M/kernel/freertos/builds/$B/release#g; s#\${CG_TOOL_ROOT}#$CGT#g")
echo "device=$DEV"; echo "flags=$FLAGS"
n=0; fail=0
for f in $(grep -oE 'file path="[^"]+\.c"' "$PS" | sed -E 's/file path="([^"]+)"/\1/' | sed "s#\${COM_TI_MSPM0_SDK_INSTALL_DIR}#$M#g"); do
  o="$O/obj/$(echo ${f#$M/} | tr '/' '_' | sed 's/\.c$/.o/')"
  eval "$CGT/bin/tiarmclang -c -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -O2 $FLAGS -I$M/kernel/freertos/builds/$B/release \"$f\" -o \"$o\"" 2>>"$O/build.log" || { fail=$((fail+1)); echo "FAIL $f" >>"$O/build.log"; }
  n=$((n+1))
done
rm -f "$O/freertos.lib"; "$CGT/bin/tiarmar" rcs "$O/freertos.lib" "$O"/obj/*.o
echo "compiled $n files, $fail failed -> $O/freertos.lib"
