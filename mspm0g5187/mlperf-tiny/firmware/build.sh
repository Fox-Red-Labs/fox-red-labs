#!/bin/bash
# Build the MLPerf Tiny keyword-spotting image for the LP-MSPM0G5187 (run from the repo root).
#   mspm0g5187/mlperf-tiny/firmware/build.sh <NNC output folder with mod.a> <out name> [extra compiler flags, e.g. -DSWD_HARNESS]
# Needs: MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig (1.28.0 used). Output: out/mspm0g5187-mlperf/build/<out name>/dut.out
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../../.."; NNC=$1; N=$2; EXTRA=${3:-}
SDK=/Applications/ti/mspm0_sdk_2_11_00_07; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SC=$(ls -d /Applications/ti/sysconfig_*/sysconfig_cli.sh | tail -1)
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
G=out/mspm0g5187-mlperf/gen; B=out/mspm0g5187-mlperf/build/$N; mkdir -p $G $B
[ -f $G/ti_msp_dl_config.c ] || { cp $HERE/mlperf80.syscfg $G/; $SC --compiler ticlang --product $SDK/.metadata/product.json --output $G $G/mlperf80.syscfg > $G/sysconfig.log 2>&1; }
G=$PWD/$G; B=$PWD/$B; NNC=$PWD/$NNC; cd $B
T=$REPO/third-party/mlperf-tiny
CF="$ARCH @$G/device.opt -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -I$T -I$NNC -Dnullptr=NULL -DMAX_DB_INPUT_SIZE=490 -DTH_MODEL_VERSION=EE_MODEL_VERSION_KWS01 $EXTRA -gdwarf-3 -Wall -Wno-unused-variable"
"$CC/bin/tiarmclang" $CF -O2 -c $HERE/th_port.c -o th.o
"$CC/bin/tiarmclang" $CF -O2 -x c -std=gnu11 -c $T/api/internally_implemented.cpp -o api.o
"$CC/bin/tiarmclang" $CF -c $G/ti_msp_dl_config.c -o cfg.o
"$CC/bin/tiarmclang" $CF -c $SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g518x_ticlang.c -o startup.o
NPULIB=; grep -q "^#define TVMGEN_DEFAULT_TI_NPU$" $NNC/tvmgen_default.h && NPULIB=$SDK/source/ti/edgeAI/lib/ticlang/m0p/npu.a
sed -E "s/--stack_size=[0-9]+/--stack_size=3072/" $G/device_linker.cmd > device_linker.cmd
"$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 th.o api.o cfg.o startup.o $NNC/mod.a $NPULIB -l$G/device.cmd.genlibs -L$SDK/source ./device_linker.cmd \
  -Wl,-m,dut.map -Wl,--rom_model -Wl,--warn_sections -L$CC/lib -llibc.a -o dut.out
"$CC/bin/tiarmsize" dut.out | tail -1 | awk '{print "text="$1" data="$2" bss="$3}'
