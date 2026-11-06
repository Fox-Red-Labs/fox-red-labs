#!/bin/bash
# Build the alarm listener for the LP-MSPM0G5187.   APP=capture|detect bash build.sh
# DETECT_FLAGS="-DSTREAM_AUDIO -DDUMP_FEATURES" APP=detect builds alarm_detect_debug (audio and features too).
# TI's SDK example folder (driverlib/spi_controller_echo_interrupts, for its makefile) with our
# alarm.c and alarm.syscfg. detect also links the NNC model library (model/nnc_g5187_npu/mod.a)
# and CMSIS-DSP for the FFT.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."; E=spi_controller_echo_interrupts
APP=${APP:-capture}
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
W=$PWD/out/alarm/build-$APP; rm -rf "$W"; mkdir -p "$W"
cp -R $SDK/examples/nortos/LP_MSPM0G5187/driverlib/$E "$W/"
cp "$HERE/alarm.c" "$W/$E/$E.c"; cp "$HERE/alarm.syscfg" "$W/$E/$E.syscfg"
rm -f "$W/$E/ti_msp_dl_config.c" "$W/$E/ti_msp_dl_config.h"
APPDEF=$(echo "APP_$APP" | tr a-z A-Z)
INC="-I$HERE ${CAPTURE_FLAGS}"; LIBS=""; EXTRA=""
if [ "$APP" = detect ] || [ "$APP" = bench ]; then
  NNC=${NNC:-$HERE/model/nnc_g5187_npu}
  INC="$INC -DARM_MATH_CM0 ${DETECT_FLAGS} -I$HERE/model -I$NNC -I$SDK/source/third_party/CMSIS/DSP/Include"
  LIBS="$NNC/mod.a $SDK/source/ti/edgeAI/lib/ticlang/m0p/npu.a $SDK/source/third_party/CMSIS/DSP/lib/ticlang/m0p/arm_cortexM0l_math.a"
  EXTRA="$HERE/features.c"
fi
M="$W/$E/ticlang/makefile"
python3 - "$M" "-D$APPDEF $INC -O2" "$LIBS" "$EXTRA" <<'PY'
import sys
m, cflags, libs, extra = sys.argv[1:]
s = open(m).read()
s = s.replace("    -O2 \\\n", "")
block = f"""
CFLAGS += {cflags}
EXTRA_SRCS = {extra}
OBJECTS += $(patsubst %.c,%.obj,$(notdir $(EXTRA_SRCS)))
$(foreach c_file,$(EXTRA_SRCS),$(eval $(call C_RULE,$(c_file))))
LFLAGS += {libs} -Wl,--stack_size=2048

$(NAME).out:"""
s = s.replace("\n$(NAME).out:", block, 1)
open(m, "w").write(s)
PY
"$REPO/mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$W/$E/ticlang" 2>&1 | grep -v "^Building\|^Generating\|generation complete" | tail -25
$CC/bin/tiarmsize "$W/$E/ticlang/$E.out"
N=alarm_$APP; [ -n "$DETECT_FLAGS" ] && N=${N}_debug
mkdir -p "$HERE/out" && cp "$W/$E/ticlang/$E.out" "$HERE/out/$N.out" && echo "image: mspm0g5187/alarm/out/$N.out"
