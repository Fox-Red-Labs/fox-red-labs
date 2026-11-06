#!/bin/bash
# Build one shared test (cc2340r5/bench/tests/<test>) for the LP-MSPM33C321A with bench/tb/tb.c as the harness.
#   build_tb.sh embench <O2|Os> <a|b|c> [mhz]     Embench group a, b or c (same three groups as the MSPM0 and CC27xx runs)
#   build_tb.sh fft O3 - [mhz]                    CMSIS-DSP prebuilt M33 library
#   build_tb.sh bitnet O3 <cnn_64|1k> [mhz]
# Output: $RUN/tb-<test>-<opt>-<variant>-<mhz>/<test>-<opt>-<mhz>/bench.out
set -e
T=$1; OPT=$2; V=$3; MHZ=${4:-160}; HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
RUN="${RUN:-$PWD/out/mspm33c321a}"; GEN="$RUN/gen$MHZ"; export GEN
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
CGT=$CC; CLANG=$CGT/bin/tiarmclang
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
LIB="$REPO/cc2340r5/bench/lib"; TBD="$HERE/bench/tb"; D="$RUN/tb-$T-$OPT-$V-$MHZ"; rm -rf "$D"; mkdir -p "$D"
export EXTRA="$TBD/tb.c"; BASEINC="-I$LIB -I$TBD"; export EXTRAFLAGS="-DTB_OPT=\"-$OPT\" -DTB_BUILD_ID=\"$T-$OPT-$V-${MHZ}mhz\""
case $T in
embench)
  E="$REPO/mspm0g3507/third-party-bench/vendor/embench-iot"
  case $V in a) L="aha-mont64 crc32 cubic edn minver nbody nsichneu slre st statemate ud";; b) L="huffbench nettle-aes nettle-sha256 picojpeg wikisort";; c) L="matmult-int qrduino sglib-combined";; esac
  O="$D/objs"; mkdir -p "$O"; : > "$O/list.h"
  "$CLANG" -$OPT -mthumb $ARCH -w -I"$E/support" -c "$E/support/beebsc.c" -o "$O/beebsc.o"
  for n in $L; do m=${n//-/_}; echo "BM($m)" >> "$O/list.h"
    for s in "$E"/src/$n/*.c; do
      "$CLANG" -$OPT -mthumb $ARCH -w -DCPU_MHZ=1 -Dbenchmark=bm_${m}_run -Dinitialise_benchmark=bm_${m}_init -Dverify_benchmark=bm_${m}_verify -Dwarm_caches=bm_${m}_warm \
        -I"$E/src/$n" -I"$E/support" -I"$E/config/arm/boards/generic" -I"$E/config/arm/chips/cortex-m4" -c "$s" -o "$O/${m}_$(basename "${s%.c}").o" >> "$O/compile.log" 2>&1
    done
    "$CGT/bin/tiarmsize" "$O/${m}_"*.o | awk -v n="$n" 'NR>1{t+=$1;d+=$2;b+=$3} END{printf "%-16s text=%d data=%d bss=%d\n", n,t,d,b}' >> "$O/sizes.txt"
  done
  rm -f "$O/bench.a"; "$CGT/bin/tiarmar" rcs "$O/bench.a" "$O"/*.o
  export EXTRAINC="$BASEINC -I$O" EXTRALIBS="$O/bench.a"; export EXTRAFLAGS="$EXTRAFLAGS -DIMAGE_NAME=\"embench_${V}_$OPT\" -w";;
fft)
  P=$SDK/source/third_party/CMSIS
  export EXTRAINC="$BASEINC -I$REPO/mspm0l1306/bench/dsp -I$REPO/cc2340r5/bench/tests/fft -I$P/DSP/Include -I$P/Core/Include"
  export EXTRAFLAGS="$EXTRAFLAGS -DARM_MATH_CM33"; export EXTRALIBS="$P/DSP/lib/ticlang/m33/arm_cortexM33_math.a";;
bitnet)
  B="$REPO/ports/bitnetmcu"; U="$REPO/third-party/bitnetmcu"
  export EXTRAINC="$BASEINC -I$B -I$U"; export EXTRAFLAGS="$EXTRAFLAGS -DMODEL_H=\"BitNetMCU_model_$V.h\" -DMODEL_NAME=$V -Wno-unused-variable";;
esac
mkdir -p "$D/run"; ln -sfn "$GEN" "$D/run/gen$MHZ"
"$HERE/tools/build_bench.sh" tb_$T $OPT $MHZ "$D/run"
ls -l "$D/run/tb_$T-$OPT-$MHZ/bench.out"
