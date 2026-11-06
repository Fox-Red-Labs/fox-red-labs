#!/bin/bash
# Build one shared test (cc2340r5/bench/tests/<test>) for the LP-AM13E230 with bench/tb/tb.c as the harness.
#   build_tb.sh embench <O2|Os> <a|b|c>      Embench group a, b or c (the same three groups as the MSPM0, CC27xx and MSPM33 runs)
#   build_tb.sh fft O3 -                     CMSIS-DSP prebuilt M33 library from the AM13E2x SDK
# Output: $RUN/tb-<test>-<opt>-<variant>/tb_<test>-<opt>/bench.out. Ported from mspm33c321a/tools/build_tb.sh.
set -e
export GEN="${RUN:-$PWD/out/am13e230}/gen-coremark"; T=$1; OPT=$2; V=$3; HERE="$(cd "$(dirname "$0")/.." && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
RUN="${RUN:-$PWD/out/am13e230}"; SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; CLANG=$CGT/bin/tiarmclang
ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
LIB="$REPO/cc2340r5/bench/lib"; TBD="$HERE/bench/tb"; D="$RUN/tb-$T-$OPT-$V"; rm -rf "$D"; mkdir -p "$D"
export STACK=${STACK:-0x3000}; export TBLOG=1; export EXTRA="$TBD/tb.c"; BASEINC="-I$LIB -I$TBD"; export EXTRAFLAGS="-DTB_OPT=\"-$OPT\" -DTB_BUILD_ID=\"$T-$OPT-$V-200mhz\""
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
  P=$SDK/source/cmsis
  export EXTRAINC="$BASEINC -I$REPO/mspm0l1306/bench/dsp -I$REPO/cc2340r5/bench/tests/fft -I$P/DSP/Include -I$P/Core/Include"
  export EXTRAFLAGS="$EXTRAFLAGS -DARM_MATH_CM33"; export EXTRALIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibCMSISDSP_m33_ti_arm_clang.a";;
esac
"$HERE/tools/build_bench.sh" tb_$T $OPT "$D"
cp "$D/tb_$T-$OPT/bench.out" "$D/bench.out" 2>/dev/null || true; ls -l "$D/tb_$T-$OPT/bench.out"
