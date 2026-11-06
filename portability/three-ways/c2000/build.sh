#!/bin/bash
# Three ways, C2000Ware DriverLib on the LAUNCHXL-F2800137: clean RAM build of common/app.c UNCHANGED + glue_c2000.c.
# No SysConfig: the board setup is C calls. Output: build-f2800137/three_ways_c2000.out, ../logs/c2000-f2800137.log.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
O="$HERE/build-f2800137"; LOG="$HERE/../logs/c2000-f2800137.log"
rm -rf "$O"; mkdir -p "$O/obj"; cd "$O/obj"
t0=$(python3 -c 'import time;print(time.time())')
CF=(--opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --gen_func_subsections=on --define=_LAUNCHXL_F2800137 --define=F280013x=1
    --define=CPU1 --define=RAM --define=generic_ram_lnk --diag_suppress=10063
    -I"$C2K" -I"$C2K/device_support/f280013x/common/include" -I"$C2K/driverlib/f280013x/driverlib" -I"$CGT/include" -I"$HERE/../common")
{
  for f in "$HERE/../common/app.c" "$HERE/glue_c2000.c" "$C2K/device_support/f280013x/common/source/device.c" \
           "$C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm"; do
    PRE=(); [ "$(basename "$f")" = app.c ] && PRE=(--preinclude="$HERE/c28x_preinclude.h")   # only app.c lacks uint8_t; driverlib's headers define it for the glue
    echo "== cl2000 $(basename "$f")"; "$CGT/bin/cl2000" "${CF[@]}" ${PRE[@]+"${PRE[@]}"} -c "$f" --output_file="$(basename "${f%.*}").obj" || exit 1
  done
  echo "== link"
  "$CGT/bin/cl2000" "${CF[@]}" -z --entry_point code_start --stack_size=0x200 --heap_size=0x100 -i"$CGT/lib" -i"$C2K" --reread_libs --rom_model \
    -m "$O/three_ways_c2000.map" -o "$O/three_ways_c2000.out" *.obj \
    "$C2K/device_support/f280013x/common/cmd/280013x_generic_ram_lnk.cmd" -l"$C2K/driverlib/f280013x/driverlib/ccs/Release/driverlib.lib" -llibc.a || exit 1
  t1=$(python3 -c 'import time;print(time.time())')
  python3 -c "print('== clean build (incl. device.c) %.1f s' % ($t1-$t0))"
}  > "$LOG" 2>&1 || { tail -30 "$LOG"; exit 1; }
tail -1 "$LOG"; echo "$O/three_ways_c2000.out"
