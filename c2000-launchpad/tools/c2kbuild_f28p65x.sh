#!/bin/bash
# Build a C2000Ware F28P65x example (or our own code) from a terminal, no CCStudio project.
#   c2kbuild_f28p65x.sh <dir with .c files and optionally one .syscfg> [FLASH|RAM] [opt level, default off] [extra cl2000 flags...]
# Output: <dir>/build-<FLASH|RAM>-O<opt>/<name>.out
# CPU1 only. Flags are the CPU1_LAUNCHXL_* configuration from TI's projectspec for this chip.
set -e
SRC=$(cd "$1" && pwd); MODE=${2:-FLASH}; OPT=${3:-off}; shift 3 2>/dev/null || shift $#
# Tool locations: TI_ROOT is the folder that holds ccs2101 and c2000ware_core_* (default /Applications/ti on macOS).
# Override any one with C2000WARE, CGT or SYSCFG. Same defaults as c2kbuild.sh.
TI_ROOT=${TI_ROOT:-/Applications/ti}
C2K=${C2000WARE:-$TI_ROOT/c2000ware_core_26_02_00_00}
CGT=${CGT:-$TI_ROOT/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS}
SYSCFG=${SYSCFG:-$TI_ROOT/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh}
for v in C2K CGT SYSCFG; do
  [ -e "${!v}" ] || { echo "$v not found: ${!v}" >&2; echo "Set TI_ROOT to the folder that holds ccs2101 and c2000ware_core_26_02_00_00, or set C2000WARE, CGT and SYSCFG to the exact paths." >&2; exit 1; }
done
NAME=$(basename "$SRC"); OUT="$SRC/build-$MODE-O$OPT"; rm -rf "$OUT"; mkdir -p "$OUT/obj"
if [ "$MODE" = FLASH ]; then DEFS="--define=_FLASH --define=generic_flash_lnk"; else DEFS="--define=RAM --define=generic_ram_lnk"; fi
CFLAGS="--opt_level=$OPT -v28 -ml -mt --float_support=fpu64 --abi=eabi --gen_func_subsections=on --define=DEBUG --define=_LAUNCHXL_F28P65X --define=CPU1 $DEFS --diag_warning=225 --diag_suppress=10063 -I$C2K -I$C2K/device_support/f28p65x/common/include -I$C2K/driverlib/f28p65x/driverlib -I$CGT/include -I$SRC -I$OUT/syscfg $*"
SC=$(ls "$SRC"/*.syscfg 2>/dev/null | head -1)
if [ "$MODE" = FLASH ]; then BASELNK=$C2K/device_support/f28p65x/common/cmd/28p65x_generic_flash_lnk_cpu1.cmd; else BASELNK=$C2K/device_support/f28p65x/common/cmd/28p65x_generic_ram_lnk_cpu1.cmd; fi
if [ -n "$SC" ]; then
  "$SYSCFG" --product "$C2K/.metadata/sdk.json" --board "/boards/LAUNCHXL_F28P65X" --device "TMS320F28P650DK" --package "169NMR" --variant "TMS320F28P650DK9" -o "$OUT/syscfg" "$SC" > "$OUT/syscfg.log" 2>&1 || { tail -20 "$OUT/syscfg.log"; exit 1; }
  GEN="$OUT/syscfg/*.c $C2K/device_support/f28p65x/common/source/device.c"
  LNK="$BASELNK"
else
  GEN="$C2K/device_support/f28p65x/common/source/device.c"
  LNK="$BASELNK"
fi
cd "$OUT/obj"
for f in "$SRC"/*.c $GEN $C2K/device_support/f28p65x/common/source/f28p65x_codestartbranch.asm; do
  [ -f "$f" ] || continue
  "$CGT/bin/cl2000" $CFLAGS -c "$f" --output_file="$(basename "${f%.*}").obj" > /dev/null || { echo "compile failed: $f"; "$CGT/bin/cl2000" $CFLAGS -c "$f" 2>&1 | tail -20; exit 1; }
done
"$CGT/bin/cl2000" $CFLAGS -z --entry_point code_start --stack_size=0x200 --heap_size=0x100 $DEFS \
  -i"$CGT/lib" -i"$C2K" -i"$OUT/syscfg" --reread_libs --rom_model -m "$OUT/$NAME.map" -o "$OUT/$NAME.out" \
  *.obj $LNK -l"$C2K/driverlib/f28p65x/driverlib/ccs/Release/driverlib.lib" -llibc.a > "$OUT/link.log" 2>&1 || { tail -25 "$OUT/link.log"; exit 1; }
echo "$OUT/$NAME.out"
