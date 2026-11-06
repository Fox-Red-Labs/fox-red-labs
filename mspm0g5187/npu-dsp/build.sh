#!/bin/bash
# Build the DSP images for the LP-MSPM0G5187. Needs the NNC output (npu-dsp/nnc.sh) and the SysConfig output
# of TI's motor_bearing_fault_detection_ai_npu example (made by mspm0g5187/prebuild.sh, "gen/npu80").
#   ./build.sh                 every model, NPU and CPU, plus the plain C baseline
#   ./build.sh fir64 bank8     only these models
# Output: out/npu-dsp/build/<model>-<npu|cpu>/dsp.out and cbase/cbase.out
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
W=$PWD/out/npu-dsp; OUT=$W/build; NNC=$W/nnc; MODELS=$W/models
G=$PWD/out/mspm0g5187/gen/npu80
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
START=startup_mspm0g518x_ticlang.c
cc() { "$CC/bin/tiarmclang" "$@"; }
CF="$ARCH @$G/device.opt -I$G -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -Wno-unused-variable -Wno-unused-const-variable"
mkdir -p "$OUT"
link() { local N=$1; shift
  sed -E "s/--stack_size=[0-9]+/--stack_size=2048/" "$G/device_linker.cmd" > device_linker.cmd
  cc $ARCH -Wl,-u,_c_int00 "$@" -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
    -Wl,-m,"$N.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$N.out" || return 1
  "$CC/bin/tiarmsize" "$N.out" | tail -1 | awk '{print "text="$1" data="$2" bss="$3}'; }
one() { # one <model> <npu|cpu>
  local M=$1 T=$2 B="$OUT/$1-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local D="$NNC/$M/nnc_g5187_${T}_int" MU; MU=$(echo "$M" | tr a-z A-Z)
  local X="-DMODEL_$MU"; [ "$T" = npu ] && X="$X -DUSE_NPU"
  cc $CF -O2 -I"$D" -I"$MODELS" $X -c "$HERE/firmware/main.c" -o main.o || return 1
  cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link dsp main.o cfg.o startup.o "$D/mod.a" $( [ "$T" = npu ] && echo "$SDK/source/ti/edgeAI/lib/ticlang/m0p/npu.a" ) ; }
base() { local B="$OUT/cbase"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  cc $CF -O3 -I"$MODELS" -c "$HERE/firmware/cbase.c" -o main.o || return 1
  cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link cbase main.o cfg.o startup.o; }
LIST=${*:-fir64 fir64_dec4 bank8 bank8_trip bank8_trip2 bank8_trip3 bank8_trips dft64}
for M in $LIST; do for T in npu cpu; do echo "== $M $T"; ( one $M $T ) 2>&1 | tail -4; done; done
echo "== cbase"; ( base ) 2>&1 | tail -4
