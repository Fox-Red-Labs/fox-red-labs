#!/bin/bash
# Build bare-metal CoreMark for the LP-MSPM0L1306 with GCC, the compiler the Zephyr runs use
# (Zephyr SDK 0.17.4, arm-zephyr-eabi-gcc 12.2.0, newlib-nano). Same CoreMark files, same main.c and
# core_portme.c, same SysConfig clock as build.sh; only the compiler, startup file and linker script change
# (the SDK's GCC ones: startup_mspm0l130x_gcc.c, mspm0l1306.lds, driverlib/lib/gcc).
#   ./build-gcc.sh <opt> <clk> [iterations]    opt: O3 O2 Os   clk: 32 or 4
#   EXTRA="-funroll-loops" ./build-gcc.sh O3 32   adds flags (folder name gets -unroll for that one)
#   VALIDATION=1 ./build-gcc.sh O3 32          EEMBC validation seeds (0x3415, 0x3415, 0x66) in place of the
#                                              performance seeds; output folder gets -val
# The stack is the top of SRAM (the SDK script reserves no fixed size); the script's link-time check is raised
# from 0x80 to STACK bytes so a build that leaves too little room for CoreMark's 2000-byte block fails to link.
# Output: build-gcc-<opt>-<clk>[-val]/coremark.elf and .map
set -e
OPT=$1; CLK=$2; ITER=${3:-1000}; STACK=${STACK:-3072}; CM_LOG=${CM_LOG:-800}
HERE="$(cd "$(dirname "$0")" && pwd)"; SDK=/Applications/ti/mspm0_sdk_2_11_00_07
GCC=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi
RUN=PERFORMANCE_RUN; SUF=""; [ "$EXTRA" = "-funroll-loops" ] && SUF=-unroll; [ "${VALIDATION:-0}" = 1 ] && { RUN=VALIDATION_RUN; SUF=$SUF-val; }
GEN="$HERE/syscfg$CLK"; OUT="$HERE/build-gcc-$OPT-$CLK$SUF"; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
sed "s/_Min_Stack_Size = 0x80;/_Min_Stack_Size = $STACK;/" "$SDK/source/ti/devices/msp/m0p/linker_files/gcc/mspm0l1306.lds" > mspm0l1306.lds
ARCH="-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft"
FLAGS="-$OPT${EXTRA:+ $EXTRA} $ARCH"
CFLAGS="$FLAGS -D__MSPM0L1306__ -D__USE_SYSCONFIG__ -I$HERE -I$GEN -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -ffunction-sections -fdata-sections -g -Wall -DITERATIONS=$ITER -D$RUN=1 -DCM_LOG_SIZE=$CM_LOG -DHAS_FLOAT=0 -include flags.h"
echo "#define COMPILER_FLAGS \"$FLAGS, bare metal, clock $CLK MHz\"" > flags.h
echo "#define COMPILER_VERSION \"GCC $($GCC-gcc -dumpversion) (Zephyr SDK 0.17.4)\"" >> flags.h
for f in core_list_join core_matrix core_state core_util core_portme main; do $GCC-gcc $CFLAGS -c "$HERE/$f.c" -o $f.o; done
$GCC-gcc $CFLAGS -Dmain=coremark_main -c "$HERE/core_main.c" -o core_main.o
$GCC-gcc $CFLAGS -c "$GEN/ti_msp_dl_config.c" -o ti_msp_dl_config.o
$GCC-gcc $CFLAGS -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0l130x_gcc.c" -o startup.o
$GCC-gcc $ARCH --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -T mspm0l1306.lds -Wl,-Map,coremark.map \
  *.o "$SDK/source/ti/driverlib/lib/gcc/m0p/mspm0l11xx_l13xx/driverlib.a" -o coremark.elf
$GCC-size coremark.elf
