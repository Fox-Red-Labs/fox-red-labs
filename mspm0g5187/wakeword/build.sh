#!/bin/bash
# Build TI's wakeword_detection_ai_npu for the LP-MSPM0G5187 on this Mac.
#   MODE=adc     TI's input (analog microphone on PB22) as shipped, plus the serial report: the control
#   MODE=i2s     the onboard microphone (ICS-40720, PCM1809, I2S on PB10 to PB12)
#   MODE=replay  samples from the Mac over the serial port into the same pipeline (no microphone)
#   MODE=shipped TI's example with only the three build fixes (no serial, nothing else changed)
#   OPT=-O2      compiler optimisation (the shipped makefile sets none, so -O0); default: none for adc and shipped, -O2 for i2s and replay
#   MIC_GAIN=<n> (i2s, replay) multiplies the microphone samples; default 1
# Writes mspm0g5187/wakeword/out/wakeword_<mode>.out
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."; E=wakeword_detection_ai_npu
MODE=${MODE:-i2s}; case $MODE in i2s|replay) OPT=${OPT--O2};; esac; OPT=${OPT:-}
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
W=$PWD/out/wakeword/build-$MODE; rm -rf "$W"; mkdir -p "$W"
cp -R $SDK/examples/nortos/LP_MSPM0G5187/edgeAI/$E "$W/"
D="$W/$E"; M="$D/ticlang/makefile"
# Build fixes the shipped makefile needs on this toolchain (friction log): warning in main, npu.c path, CMSIS-DSP library
sed -i '' 's/    -Wall$/    -Wall -Wno-incompatible-pointer-types/' "$M"
sed -i '' 's#^ti_edgeAI_npu_npu.obj: /home[^ ]*npu.c#ti_edgeAI_npu_npu.obj: $(MSPM0_SDK_INSTALL_DIR)/source/ti/edgeAI/npu/npu.c#' "$M"
sed -i '' 's#^LFLAGS += -lmodel/model.a#LFLAGS += $(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/DSP/lib/ticlang/m0p/arm_cortexM0l_math.a -lmodel/model.a#' "$M"
if [ "$MODE" != shipped ]; then
  cp "$HERE/wakeword_detection_ai_npu.c" "$HERE/frl_report.c" "$HERE/frl_report.h" "$HERE/fir_hb.h" "$D/"
  sed -i '' 's/^OBJECTS = /OBJECTS = frl_report.obj /' "$M"
  python3 - "$M" <<'PY'
import sys
m=sys.argv[1]; s=open(m).read()
rule='''
frl_report.obj: ../frl_report.c $(SYSCFG_H_FILES)
	@ echo Building $@
	$(V) $(CC) $(CFLAGS) -c $< -o $@
'''
s=s.replace("\nti_edgeAI_npu_npu.obj:", rule+"\nti_edgeAI_npu_npu.obj:",1)
open(m,'w').write(s)
PY
  case $MODE in
    adc)    DEF="-DINPUT_ADC=1"; cp "$HERE/wakeword_adc.syscfg" "$D/$E.syscfg"
            # measurement hook: count the samples the shipped ADC interrupt takes
            sed -i '' 's/^            samples_count++;/            samples_count++; frl_isr_count++;/; s/^volatile uint32_t adc_overflow = 0;/volatile uint32_t adc_overflow = 0; extern volatile uint32_t frl_isr_count;/' "$D/data_collection.c" ;;
    i2s)    DEF="-DINPUT_I2S=1 -DMIC_GAIN=${MIC_GAIN:-1}"; sed 's/@BAUD@/921600/' "$HERE/wakeword_i2s.syscfg" > "$D/$E.syscfg"; cp "$HERE/data_collection.c" "$D/data_collection.c" ;;
    replay) DEF="-DINPUT_REPLAY=1 -DMIC_GAIN=${MIC_GAIN:-1}"; sed 's/@BAUD@/230400/' "$HERE/wakeword_i2s.syscfg" > "$D/$E.syscfg"; cat "$HERE/replay_uart_dma.syscfg" >> "$D/$E.syscfg"; cp "$HERE/data_collection.c" "$D/data_collection.c" ;;
  esac
  sed -i '' "s/^CFLAGS += -I.. \\\\/CFLAGS += $DEF $OPT -I.. \\\\/" "$M"
fi
[ "$MODE" = shipped ] && [ -n "$OPT" ] && sed -i '' "s/^CFLAGS += -I.. \\\\/CFLAGS += $OPT -I.. \\\\/" "$M"
"$REPO/mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$D/ticlang" 2>&1 | grep -E "error|undefined|warning: unused|Error" | head -20 || true
OUT="$D/ticlang/$E.out"; [ -f "$OUT" ] || { echo BUILD FAILED; exit 1; }
$CC/bin/tiarmsize "$OUT"
mkdir -p "$HERE/out" && N=wakeword_$MODE; [ -n "$OPT" ] && N=${N}_${OPT#-}; cp "$OUT" "$HERE/out/$N.out" && cp "$D/ticlang/$E.map" "$HERE/out/$N.map" && echo "image: mspm0g5187/wakeword/out/$N.out"
