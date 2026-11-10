#!/bin/bash
# Build one port for the LAUNCHXL-F2800137 from the terminal: TI C2000 compiler, driverlib, the Flash API library and its linker file.
#   ./build_f28.sh <project> [-DFLAG ...]       e.g. ./build_f28.sh fram-persist -DBURST=300
# Reads <project>/f2800137/main.c. Output: $OUT/<project><tag>/<project>.out (OUT defaults to out/msp430-ports/f28).
# Flags follow c2000-launchpad/tools/c2kbuild.sh (FLASH mode); the linker file is C2000Ware's 280013x_flash_api_lnk.cmd, which loads the Flash API
# from flash and runs it from RAM. C2000Ware's Flash API sits in the full install (libraries/flash_api/f280013x); FAPI=<folder> points elsewhere.
set -eu
P="$1"; shift
HERE="$(cd "$(dirname "$0")" && pwd)"
TAG=""; for a in "$@"; do TAG="$TAG$(echo "$a" | tr -c 'A-Za-z0-9=\n' '_')"; done
OUT="${OUT:-$PWD/out/msp430-ports/f28}/$P$TAG"
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
FAPI=${FAPI:-/Applications/ti/c2000/C2000Ware_26_02_00_00/libraries/flash_api/f280013x}
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
rm -rf "$OUT"; mkdir -p "$OUT/obj"
CFLAGS="--c99 --opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --gen_func_subsections=on --define=DEBUG --define=_LAUNCHXL_F2800137 --define=F280013x=1 --define=CPU1 --define=_FLASH --define=generic_flash_lnk --diag_warning=225 --diag_suppress=10063 -I$C2K -I$C2K/device_support/f280013x/common/include -I$C2K/driverlib/f280013x/driverlib -I$FAPI/include/FlashAPI -I$CGT/include -I$HERE/common -I$HERE/$P/f2800137 $*"
cd "$OUT/obj"
for f in "$HERE/$P/f2800137/main.c" "$C2K/device_support/f280013x/common/source/device.c" "$C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm"; do
  "$CGT/bin/cl2000" $CFLAGS -c "$f" --output_file="$(basename "${f%.*}").obj" >> "$OUT/build.log" 2>&1 || { grep -m10 -i "error" "$OUT/build.log"; exit 1; }
done
"$CGT/bin/cl2000" $CFLAGS -z --entry_point code_start --stack_size=0x200 --heap_size=0x100 -i"$CGT/lib" -i"$C2K" -i"$FAPI/lib" --reread_libs --rom_model \
  -m "$OUT/$P.map" -o "$OUT/$P.out" *.obj "$C2K/device_support/f280013x/common/cmd/280013x_flash_api_lnk.cmd" \
  -l"$C2K/driverlib/f280013x/driverlib/ccs/Release/driverlib.lib" -lFAPI_F280013x_EABI_v2.00.10.lib -llibc.a >> "$OUT/build.log" 2>&1 || { tail -15 "$OUT/build.log"; exit 1; }
echo "built $OUT/$P.out: $(grep -E 'FLASH_BANK0|RAMLS|RAMM' "$OUT/$P.map" | head -0)"
