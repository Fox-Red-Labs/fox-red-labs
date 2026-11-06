#!/bin/bash
# Build everything for the NXP FRDM-MCXA153 that needs no board: NXP's led_blinky and hello_world, CoreMark
# (unmodified EEMBC files, third-party/coremark/) at -O3, -O2, -Os, the four occupancy models, math and Dhrystone
# on the same harness as the TI boards, the build pass of every SDK example for the board (the sweep), and
# Zephyr's blinky. Output: out/frdm-mcxa153/
#
#   ./prebuild.sh          refuse to start if the Mac is busy (1-minute load at or above the core count)
#   WAIT=1 ./prebuild.sh   wait for a quiet Mac instead
#
# Installs nothing. When the SDK is missing it prints the install steps, with the versions and sizes read on
# NXP's pages on 2026-10-07, and exits 2. Never touches a board.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/frdm-mcxa153}"
WS="${MCUX_WS:-$HOME/mcuxsdk}"                                # west workspace from the install steps
SDK="$WS/mcuxsdk"
WEST="${MCUX_WEST:-$WS/.venv/bin/west}"
ARM_PFX="${ARM_PFX:-arm-zephyr-eabi}"   # arm-none-eabi with ARM_TC pointing at the Arm GNU Toolchain
ARM_TC="${ARM_TC:-$HOME/zephyr-toolchain/arm-zephyr-eabi}"    # GCC 12.2.0 from Zephyr SDK 0.17.4, already here
BOARD=frdmmcxa153
ZEPHYR="$HOME/zephyrproject"

CORES=$(sysctl -n hw.ncpu)
load1() { sysctl -n vm.loadavg | awk '{print int($2)}'; }
if [ "${IGNORE_LOAD:-0}" = 1 ]; then :   # the Mac is shared and always loaded: build niced instead of waiting
elif [ "${WAIT:-0}" = 1 ]; then
	while [ "$(load1)" -ge "$CORES" ]; do sleep 30; done
elif [ "$(load1)" -ge "$CORES" ]; then
	echo "prebuild: Mac is busy (1-minute load $(load1), $CORES cores). Not starting. WAIT=1 to wait." >&2
	exit 1
fi

missing=""
[ -d "$SDK/examples/_boards/$BOARD" ] || missing="$missing sdk"
[ -x "$WEST" ] || missing="$missing west"
[ -x "$ARM_TC/bin/$ARM_PFX-gcc" ] || missing="$missing armgcc"
command -v ninja >/dev/null || missing="$missing ninja"
if [ -n "$missing" ]; then
	cat <<EOF
prebuild: missing:$missing. Nothing built. Install steps (read 2026-10-07; recheck the versions first):

  MCUXpresso SDK 26.09.00 (manifest tag v26.09.00, 2026-09-30), GitHub route, no NXP account:
    python3 -m venv $WS/.venv && $WS/.venv/bin/pip install -U west
    $WS/.venv/bin/west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git --mr v26.09.00 $WS
    cd $WS && .venv/bin/west update_board --set board $BOARD
    .venv/bin/pip install -r mcuxsdk/scripts/requirements.txt
  NXP's guide: 1 to 2 GB for one board, about 7 GB for the whole SDK. (The SDK Builder zip on
  mcuxpresso.nxp.com is the other route and needs an NXP account.)
  LinkServer 26.9.130 (2026-09-21), macOS Intel PKG, 32,144 kB, from nxp.com/linkserver (the page shows
    "Account Required"). Flashing only; the builds below do not need it.
  Compiler: none to install. This script points ARMGCC_DIR at links named arm-none-eabi-* to
    $ARM_TC (GCC 12.2.0).
  CMake 3.30 and Ninja 1.12.1 recommended by NXP: CMake from $ZEPHYR/.venv/bin, /usr/local/bin/ninja.
  Zephyr rows: cd $ZEPHYR && .venv/bin/west update hal_nxp   (402 MB on GitHub)

  Free disk now: $(df -h "$HOME" | awk 'NR==2{print $4}').
EOF
	exit 2
fi

mkdir -p "$OUT"; LOG="$OUT/build.log"; : > "$LOG"
echo "prebuild frdm-mcxa153: $(date '+%Y-%m-%d %H:%M:%S'), load $(uptime | sed 's/.*load averages*: //')" | tee -a "$LOG"
t0=$(date +%s)

# 1. ARMGCC_DIR shim: the SDK looks for bin/arm-none-eabi-gcc; ours is named arm-zephyr-eabi-gcc
SHIM="$OUT/armgcc"; mkdir -p "$SHIM/bin"
for t in "$ARM_TC"/bin/$ARM_PFX-*; do ln -sf "$t" "$SHIM/bin/arm-none-eabi-${t##*/$ARM_PFX-}"; done
export ARMGCC_DIR="$SHIM"
export PATH="$ZEPHYR/.venv/bin:$PATH"   # CMake

mbuild() { # mbuild <example path under examples/> <build dir>
	(cd "$WS" && "$WEST" build -b "$BOARD" "mcuxsdk/examples/$1" --toolchain armgcc --config release -d "$2" -p always) >>"$LOG" 2>&1
}

# 2. NXP's blink and hello_world (the hello_world build is also the base the bench images link against)
if [ "${BENCH_ONLY:-0}" != 1 ]; then   # BENCH_ONLY=1 rebuilds only the bench images on the existing hello_world build
mbuild demo_apps/led_blinky "$OUT/led_blinky"; echo "led_blinky: exit $?" | tee -a "$LOG"
mbuild demo_apps/hello_world "$OUT/hello_world"; echo "hello_world: exit $?" | tee -a "$LOG"
mbuild freertos_examples/freertos_hello "$OUT/freertos_hello"; echo "freertos_hello: exit $?" | tee -a "$LOG"
mbuild driver_examples/lpadc/edma "$OUT/lpadc_edma"; echo "lpadc_edma: exit $?" | tee -a "$LOG"
fi

# 3. Bench images on the shared harness (cc2340r5/bench/tests, tb.h), linked against hello_world's
#    board setup by bench/sdk_reuse.py
CM="$REPO/third-party/coremark"; OCC="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"
TESTS="$REPO/cc2340r5/bench/tests"; TBH="$REPO/cc2340r5/bench/lib"; DHRY="$REPO/mspm0l1306/bench/dhrystone/src"
LIBS="$HERE/bench/lib/tb_mcx.c $HERE/bench/lib/main_mcx.c"
INC="-I$TBH -I$HERE/bench/coremark -I$CM -I$OCC"
GITID=$(git -C "$REPO" rev-parse --short HEAD)
img() { # img <name> <opt> <extra flags> -- sources
	local n=$1 o=$2 x=$3; shift 4
	# CoreMark keeps its data on the stack (6 kB stack) and the reports print floats: both need linker flags
	SDK_REUSE_LDFLAGS="-Wl,--defsym=__stack_size__=0x1800 -Wl,-u,_printf_float ${LD_EXTRA:-}" python3 -I "$HERE/bench/sdk_reuse.py" "$OUT/hello_world" "$OUT/bench/$n.elf" "$o" $INC $x \
		"-DTB_BUILD_ID=\"$n-$GITID\"" "-DTB_OPT=\"$o\"" -- "$@" $LIBS >>"$LOG" 2>&1
	echo "bench $n: exit $?" | tee -a "$LOG"
}
mkdir -p "$OUT/bench"
if [ -f "$OUT/hello_world/compile_commands.json" ]; then
	img coremark_O3_48mhz -O3 "-DBENCH_BOOT48 -DITERATIONS=4000 -DPERFORMANCE_RUN=1 -DCOMPILER_FLAGS=\"-O3,MCUXpresso_SDK_26.09.00_release\"" -- \
		$CM/core_list_join.c $CM/core_matrix.c $CM/core_state.c $CM/core_util.c "$CM/core_main.c@-Dmain=coremark_main" \
		"$HERE/bench/coremark/core_portme.c"
	for o in O3 O2 Os; do
		img coremark_$o -$o "-DITERATIONS=4000 -DPERFORMANCE_RUN=1 -DCOMPILER_FLAGS=\"-$o,MCUXpresso_SDK_26.09.00_release\"" -- \
			$CM/core_list_join.c $CM/core_matrix.c $CM/core_state.c $CM/core_util.c "$CM/core_main.c@-Dmain=coremark_main" \
			"$HERE/bench/coremark/core_portme.c"
	done
	for o in O3 Os; do
		img ai_occ_c_$o -$o "-Wno-error=misleading-indentation" -- $TESTS/ai_occ/main_c.c $OCC/occ_cnn_500/occ_cnn_500.c $OCC/occ_cnn_2k/occ_cnn_2k.c \
			$OCC/occ_cnn_8k/occ_cnn_8k.c $OCC/occ_cnn_30k/occ_cnn_30k.c
	done
	img math_O3 -O3 "" -- $TESTS/math/main.c
	img timing_O3 -O3 "" -- $HERE/bench/tests/timing/main.c
	if [ -f "$OUT/freertos_hello/compile_commands.json" ]; then
		( export SDK_TEMPLATE=freertos_hello
		python3 -I "$HERE/bench/sdk_reuse.py" "$OUT/freertos_hello" "$OUT/bench/rtos_O2.elf" -O2 -I$TBH "-DTB_USE_DWT" "-DTB_BUILD_ID=\"rtos_O2-$GITID\"" "-DTB_OPT=\"-O2\"" -- $HERE/bench/tests/rtos/main.c $LIBS >>"$LOG" 2>&1
		echo "bench rtos_O2: exit $?" | tee -a "$LOG" )
	fi
	for o in O2 O3; do img foc_$o -$o "-I$REPO/c2000-launchpad/bench/ctrl -lm" -- $HERE/bench/tests/foc/main.c $REPO/c2000-launchpad/bench/ctrl/foc.c; done
	RT="$SDK/middleware/rtcesl/CM33NODSP_RTCESL_4.8.1_MCUX"
	( export LD_EXTRA="-L$RT/GFLIB -L$RT/GMCLIB -L$RT/MLIB -Wl,--start-group -lGFLIB -lGMCLIB -lMLIB -Wl,--end-group"
	img foc_rtcesl_O2 -O2 "-I$RT/GFLIB/Include -I$RT/GMCLIB/Include -I$RT/MLIB/Include" -- $HERE/bench/tests/foc_rtcesl/main.c )
	for m in cnn_64 cnn_16small 12k 1k; do
		img bitnet_${m}_O3 -O3 "-I$REPO/ports/bitnetmcu -I$REPO/third-party/bitnetmcu -DMODEL_H=\"BitNetMCU_model_$m.h\"" -- $HERE/bench/tests/bitnet/main.c
	done
	if [ -f "$OUT/lpadc_edma/compile_commands.json" ]; then
		( export SDK_TEMPLATE=lpadc_edma
		for t in RATE LOOP; do
			python3 -I "$HERE/bench/sdk_reuse.py" "$OUT/lpadc_edma" "$OUT/bench/adc_$t.elf" -O2 -I$TBH -I$REPO/c2000-launchpad/bench/ctrl -I$SDK/drivers/lptmr -DADC_TEST_$t "-DTB_BUILD_ID=\"adc_$t-$GITID\"" "-DTB_OPT=\"-O2\"" -- $HERE/bench/tests/adc/main.c $REPO/c2000-launchpad/bench/ctrl/foc.c $LIBS >>"$LOG" 2>&1
			echo "bench adc_$t: exit $?" | tee -a "$LOG"
		done )
	fi
	img crc_O2 -O2 "-I$SDK/drivers/crc" -- $HERE/bench/tests/crc/main.c $SDK/drivers/crc/fsl_crc.c
	DR="@-Dmain=dhry_main@-Dprintf=dh_printf@-Dscanf=dh_scanf@-Dmalloc=dh_malloc@-include@string.h@-std=gnu89@-w"
	img dhrystone_O3 "-O3" "-fno-inline -DTIME -DDH_RUNS=1000000 -DDH_RULES=\"followed\" -I$DHRY" -- \
		"$DHRY/dhry_1.c$DR" "$DHRY/dhry_2.c$DR" $TESTS/dhrystone/main.c
else
	echo "bench: skipped, hello_world did not build (no compile_commands.json)" | tee -a "$LOG"
fi
# The occupancy harness runs at -O2 on the TI boards; here the whole image takes one level (say so in conditions).

# 4. Sweep build pass: every example folder NXP ships for this board
if [ "${BENCH_ONLY:-0}" != 1 ] && [ "${SKIP_SWEEP:-0}" != 1 ]; then
SW="$OUT/sweep"; mkdir -p "$SW"; [ -s "$OUT/sweep-build.csv" ] && [ "${RESUME:-0}" = 1 ] || echo "example,exit,seconds,text/data/bss" > "$OUT/sweep-build.csv"
( cd "$SDK/examples/_boards/$BOARD" && find . -mindepth 2 -name prj.conf | xargs -n1 dirname | sort -u ) |
while read -r rel; do
	rel=${rel#./}; [ -d "$SDK/examples/$rel" ] || continue
	grep -q "^$rel," "$OUT/sweep-build.csv" && continue
	free=$(df -k / | tail -1 | awk '{print $4}')
	if [ "$free" -lt 4000000 ]; then echo "sweep: stopped, free disk $free kB under 4 GB" | tee -a "$LOG"; break; fi
	d="$SW/${rel//\//__}"; s=$(date +%s); mbuild "$rel" "$d"; e=$?
	# keep only the size of the image, then delete the build tree (one tree on disk at a time)
	sz=$(find "$d" -name '*.elf' -maxdepth 2 -exec "$ARM_TC/bin/$ARM_PFX-size" {} \; 2>/dev/null | awk 'NR==2{print $1"/"$2"/"$3}')
	echo "$rel,$e,$(( $(date +%s) - s )),$sz" >> "$OUT/sweep-build.csv"
	rm -rf "$d"
done
echo "sweep: $(grep -c ',0,' "$OUT/sweep-build.csv") of $(( $(wc -l < "$OUT/sweep-build.csv") - 1 )) built" | tee -a "$LOG"
fi

# 5. Zephyr blinky, if the NXP HAL module is cloned (do not run west update from this script)
if [ -d "$ZEPHYR/modules/hal/nxp" ]; then
	(cd "$ZEPHYR" && ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-" .venv/bin/west build -p always -b frdm_mcxa153 zephyr/samples/basic/blinky -d "$OUT/zephyr-blinky") >>"$LOG" 2>&1
	echo "zephyr blinky: exit $?" | tee -a "$LOG"
else
	echo "zephyr: skipped, hal_nxp not cloned (west update hal_nxp)" | tee -a "$LOG"
fi

# 6. Sizes
{
	echo "image,text,data,bss"
	for e in "$OUT"/led_blinky/*.elf "$OUT"/bench/*.elf "$OUT"/zephyr-blinky/zephyr/zephyr.elf; do
		[ -f "$e" ] && "$ARM_TC/bin/$ARM_PFX-size" "$e" | awk -v n="$e" 'NR==2{print n","$1","$2","$3}'
	done
} > "$OUT/sizes.csv"
echo "prebuild frdm-mcxa153: done in $(( $(date +%s) - t0 )) s; sizes in $OUT/sizes.csv; log $LOG" | tee -a "$LOG"
