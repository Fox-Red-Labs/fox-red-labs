#!/bin/bash
# Build bare-metal CoreMark for the LP-MSPM0L1306 with TI Arm Clang.
#   ./build.sh <opt> <clk> [iterations]    opt: O3 O2 Os Oz   clk: 32 or 4
# 4 KB of SRAM: CoreMark keeps its 2000-byte data block on the stack (MEM_STACK), so the stack is
# raised from the example's 256 bytes to STACK bytes, and the RAM log is CM_LOG bytes.
#   VALIDATION=1 ./build.sh O3 32   EEMBC validation seeds (0x3415, 0x3415, 0x66); output folder gets -val
# Output: build-<opt>-<clk>[-val]/coremark.out and .map
set -e
OPT=$1; CLK=$2; ITER=${3:-1000}; STACK=${STACK:-3072}; CM_LOG=${CM_LOG:-800}
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../../.." && pwd)"
# the SDK and TI Arm Clang found the way tools/frl finds them (TI's install folders, or FRL_MSPM0_SDK and FRL_TICLANG)
SDK=""; CC=""
while IFS= read -r -d '' a; do case "$a" in MSPM0_SDK_INSTALL_DIR=*) SDK=${a#*=};; TICLANG_ARMCOMPILER=*) CC=${a#*=};; esac
done < <(python3 "$ROOT/tools/frl/toolenv.py" mspm0-cc)
[ -n "$SDK" ] && [ -n "$CC" ] || exit 2
RUN=PERFORMANCE_RUN; SUF=""; [ "${VALIDATION:-0}" = 1 ] && { RUN=VALIDATION_RUN; SUF=-val; }
GEN="$HERE/syscfg$CLK"; OUT="$HERE/build-$OPT-$CLK$SUF"; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
sed "s/--stack_size=256/--stack_size=$STACK/" "$GEN/device_linker.cmd" > device_linker.cmd
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
FLAGS="-$OPT $ARCH"
CFLAGS="$FLAGS @$GEN/device.opt -I$HERE -I$GEN -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall \
 -DITERATIONS=$ITER -D$RUN=1 -DCM_LOG_SIZE=$CM_LOG -DHAS_FLOAT=${HAS_FLOAT:-0} -include flags.h"
echo "#define COMPILER_FLAGS \"$FLAGS, bare metal, clock $CLK MHz\"" > flags.h
echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\"" >> flags.h
for f in core_list_join core_matrix core_state core_util core_portme main; do "$CC/bin/tiarmclang" $CFLAGS -c "$HERE/$f.c" -o $f.o; done
"$CC/bin/tiarmclang" $CFLAGS -Dmain=coremark_main -c "$HERE/core_main.c" -o core_main.o
"$CC/bin/tiarmclang" $CFLAGS -c "$GEN/ti_msp_dl_config.c" -o ti_msp_dl_config.o
"$CC/bin/tiarmclang" $CFLAGS -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0l130x_ticlang.c" -o startup.o
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
  -Wl,-m,coremark.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o coremark.out
grep -E '^ +(FLASH|SRAM) ' coremark.map
