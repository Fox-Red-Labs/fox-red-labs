#!/bin/bash
# Build a C2000Ware F280013x example (or our own code) from a terminal, no CCStudio project.
#   c2kbuild.sh <dir with .c files and optionally one .syscfg> [FLASH|RAM] [opt level, default off] [extra cl2000 flags...]
# Output: <dir>/build-<FLASH|RAM>-O<opt>/<name>.out
# Flags are the CPU1_LAUNCHXL_* configuration from TI's projectspec; --tmu_support is not in it.
set -e
SRC=$(cd "$1" && pwd); MODE=${2:-FLASH}; OPT=${3:-off}; shift 3 2>/dev/null || shift $#
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
SYSCFG=/Applications/ti/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh
NAME=$(basename "$SRC"); OUT="$SRC/build-$MODE-O$OPT"; rm -rf "$OUT"; mkdir -p "$OUT/obj"
if [ "$MODE" = FLASH ]; then DEFS="--define=_FLASH --define=generic_flash_lnk"; else DEFS="--define=RAM --define=generic_ram_lnk"; fi
CFLAGS="--opt_level=$OPT -v28 -ml -mt --float_support=fpu32 --abi=eabi --gen_func_subsections=on --define=DEBUG --define=_LAUNCHXL_F2800137 --define=F280013x=1 --define=CPU1 $DEFS --diag_warning=225 --diag_suppress=10063 -I$C2K -I$C2K/device_support/f280013x/common/include -I$C2K/driverlib/f280013x/driverlib -I$CGT/include -I$SRC -I$OUT/syscfg $*"
SC=$(ls "$SRC"/*.syscfg 2>/dev/null | head -1)
if [ -n "$SC" ]; then
  "$SYSCFG" --product "$C2K/.metadata/sdk.json" -o "$OUT/syscfg" "$SC" > "$OUT/syscfg.log" 2>&1 || { tail -20 "$OUT/syscfg.log"; exit 1; }
  GEN="$OUT/syscfg/*.c"; LNK="$OUT/syscfg/device_cmd.cmd"
  [ -f "$LNK" ] || LNK=""
else
  GEN="$C2K/device_support/f280013x/common/source/device.c"; LNK=""
fi
if [ -z "$LNK" ]; then
  if [ "$MODE" = FLASH ]; then LNK=$C2K/device_support/f280013x/common/cmd/280013x_generic_flash_lnk.cmd; else LNK=$C2K/device_support/f280013x/common/cmd/280013x_generic_ram_lnk.cmd; fi
fi
cd "$OUT/obj"
for f in "$SRC"/*.c $GEN $C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm; do
  [ -f "$f" ] || continue
  "$CGT/bin/cl2000" $CFLAGS -c "$f" --output_file="$(basename "${f%.*}").obj" > /dev/null || { echo "compile failed: $f"; "$CGT/bin/cl2000" $CFLAGS -c "$f" 2>&1 | tail -20; exit 1; }
done
"$CGT/bin/cl2000" $CFLAGS -z --entry_point code_start --stack_size=0x200 --heap_size=0x100 $DEFS \
  -i"$CGT/lib" -i"$C2K" -i"$OUT/syscfg" --reread_libs --rom_model -m "$OUT/$NAME.map" -o "$OUT/$NAME.out" \
  *.obj "$LNK" -l"$C2K/driverlib/f280013x/driverlib/ccs/Release/driverlib.lib" -llibc.a > "$OUT/link.log" 2>&1 || { tail -25 "$OUT/link.log"; exit 1; }
echo "$OUT/$NAME.out"
