#!/bin/bash
# Post-quantum image (bench/pqc/main.c + portability/pqc-join/core + mlkem-native + mldsa-native) for the LP-MSPM33C321A.
#   build_pqc.sh <O3|Os|O2> [mhz]    output <dir>/pqc-<opt>-<mhz>/bench.out  (+ bench.map); stack 64 kB (ML-DSA-44 signing needs 51 kB)
# Build only: never touches a probe. Reads the SysConfig output in $RUN/gen<mhz> (prebuild.sh makes it).
set -e
OPT=${1:-O3}; MHZ=${2:-160}; HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
R="${RUN:-$PWD/out/speed-run/mspm33c321a}"; G="$R/gen$MHZ"; O="$R/pqc-$OPT-$MHZ"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
CLANG=$CC/bin/tiarmclang
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
rm -rf "$O"; mkdir -p "$O"; cd "$O"
sed -E "s/^--stack_size=[0-9]+/--stack_size=65536/" "$G/device_linker.cmd" > device_linker.cmd
BID="mspm33c321a-pqc-$OPT-$MHZ-$(date +%Y%m%d%H%M%S)"
CF="-$OPT $ARCH @$G/device.opt -I$G -I$HERE/bench/pqc -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -DCPU_HZ=${MHZ}000000u"
MLF="-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I$ML -I$PJ"
MDF="-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API -I$MD -I$PJ"
c() { "$CLANG" $CF "$@"; }
SU="$SDK/source/ti/devices/msp/m33/startup_system_files/ticlang/startup_mspm33c321x_ticlang.c"
c -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"-$OPT\"" -I"$PJ" -I"$MD" $MLF -c "$HERE/bench/pqc/main.c" -o main.o
c -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
c -c "$SU" -o startup.o
c $MLF -c "$ML/mlkem_native.c" -o mlkem_native.o
c $MLF -c "$PJ/pqc_join.c" -o pqc_join.o
c -c "$PJ/pqc_sym.c" -o pqc_sym.o
c $MDF -c "$MD/mldsa_native.c" -o mldsa_native.o
c $MDF -c "$PJ/pqc_sign.c" -o pqc_sign.o
"$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o -l"$G/device.cmd.genlibs" "-L$SDK/source" device_linker.cmd -Wl,-m,bench.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o bench.out
grep -E '^ +(BCR|BSL)_CONFIG ' bench.map | awk '$4!="00000000"{print "NONMAIN USED"; exit 1}' || true
echo "$BID" > build-id.txt
"$CC/bin/tiarmsize" bench.out | tail -1
for o in mlkem_native pqc_join pqc_sym mldsa_native pqc_sign; do printf "%s: " $o; "$CC/bin/tiarmsize" $o.o | tail -1 | awk '{print "text="$1" data="$2" bss="$3}'; done
echo "built $O/bench.out ($BID)"
