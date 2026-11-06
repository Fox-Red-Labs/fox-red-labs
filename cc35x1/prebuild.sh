#!/bin/bash
# Build everything for the LP-EM-CC35X1 that needs no board, before it arrives. NOT RUN YET (written 2026-10-07).
# Touches no probe, serial port or board, and never runs the Wi-Fi Toolbox programmer. Writes only under
# $FRL_OUT (default ./build-cc35x1 in the folder you run it from).
#   ./prebuild.sh          refuse if the Mac is busy (1-minute load average at or above the core count)
#   ./prebuild.sh --wait   wait for a quiet Mac instead
# Steps (logs in $RUN/logs, summary in $RUN/summary.txt):
#   1 blink     SDK drivers/empty (FreeRTOS; toggles the red LED, GPIO34), then the Toolbox image builder (signs
#               the images with the SDK example key; host-side only, burns nothing)
#   2 coremark  third-party/coremark (unmodified) + cc35x1/coremark port in a copy of drivers/uart2echo, -O3 and -Oz
#   3 occupancy the four room-occupancy models compiled for the Cortex-M33 at -Os and -O3 (sizes; the run harness
#               is written at arrival)
#   4 sweep     build pass of all 32 SDK examples for LP_EM_CC35X1 with the paths this SDK needs
#   5 mqtt      demos/mqtt_client with the SDK's placeholder network name (the real one goes in a copy outside the repo)
#   6 brhost    the OpenThread border-router host libraries (border-router/host/build.sh, milestone M0)
# Why not tools/board_sweep/build_all.py: it passes SysConfig 1.26.3 and no Toolbox path; this SDK's makefiles need
# SysConfig 1.28 (SDK release notes) and SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR (its imports.mak names a 4.3.18 folder
# that is not installed; 4.3.23 is).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
RUN="${FRL_OUT:-$PWD/build-cc35x1}"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39
TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS   # SDK tested with 4.0.4.LTS; 5.1.1 is what is installed
CORES=$(sysctl -n hw.ncpu)
busy() { [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; }
if busy; then
	if [ "${1:-}" = "--wait" ]; then
		while busy; do sleep 30; done
	else
		echo "Mac is busy: $(uptime). 1-minute load is at or above $CORES cores. Builds now would skew any timed run."
		echo "Run again later, or with --wait."; exit 3
	fi
fi
for p in "$SDK" "$TB/simplelink-wifi-toolbox" "$SYSCFG" "$CGT/bin/tiarmclang"; do [ -e "$p" ] || { echo "missing: $p"; exit 4; }; done
mkdir -p "$RUN/logs"; S="$RUN/summary.txt"; echo "prebuild cc35x1 $(date '+%Y-%m-%d %H:%M:%S'), load: $(uptime)" > "$S"
step() { echo "== $1: $2" | tee -a "$S"; }
MK=(SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER=)
EX="$SDK/examples/rtos/LP_EM_CC35X1"

# build_one <example dir> <dest>: copy, make with the paths above; prints ok or the first error
build_one() {
	rm -rf "$2"; mkdir -p "$(dirname "$2")"; cp -R "$1" "$2"
	local t; t=$(find "$2" -path '*ticlang/makefile' | head -1); [ -n "$t" ] || { echo "no ticlang makefile"; return 1; }
	make -C "$(dirname "$t")" "${MK[@]}" > "$2.log" 2>&1 && echo ok || { grep -m1 -iE "error|not found|No rule" "$2.log" | cut -c1-200; return 1; }
}

# 1 blink, then the Toolbox's image builder (CCStudio post-build step, run by hand: toolbox scripts/README.md)
B="$RUN/blink/empty"; r=$(build_one "$EX/drivers/empty" "$B"); step blink "$r"
if [ "$r" = ok ]; then
	d="$B/freertos/ticlang"
	make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$d" BUILD_DIR="$d" BUILD_ARTIFACT="$d/empty.out" TOOLBOX_DIR="$TB" \
		> "$RUN/logs/blink-images.txt" 2>&1 && step blink-images "ok: $(ls "$d" | grep -iE '\.bin$|\.json$' | tr '\n' ' ')" \
		|| step blink-images "FAILED, see logs/blink-images.txt"
fi

# 2 CoreMark in a copy of drivers/uart2echo
for o in O3 Oz; do
	C="$RUN/coremark/$o"; rm -rf "$C"; mkdir -p "$RUN/coremark"; cp -R "$EX/drivers/uart2echo" "$C"
	cp "$HERE/coremark/cm_run.c" "$C/uart2echo.c"
	sed -i '' 's/^#define THREADSTACKSIZE 1024/#define THREADSTACKSIZE 4096/' "$C/freertos/main_freertos.c"
	M="$C/freertos/ticlang/makefile"
	sed -i '' -e "s#^OBJECTS = uart2echo.obj#OBJECTS = uart2echo.obj core_list_join.obj core_main.obj core_matrix.obj core_state.obj core_util.obj core_portme.obj#" \
		-e "s#^    -Oz \\\\#    -$o -I$REPO/third-party/coremark -I$HERE/coremark -DPERFORMANCE_RUN=1 -DITERATIONS=8000 \\\\#" "$M"
	cat >> "$M" <<MAKE

# added by cc35x1/prebuild.sh: EEMBC CoreMark (unmodified) and its CC35X1 port
vpath %.c $REPO/third-party/coremark $HERE/coremark
core_main.obj: CFLAGS += -Dmain=coremark_main
core_%.obj: core_%.c \$(SYSCFG_H_FILES)
	\$(CC) \$(CFLAGS) -c \$< -o \$@
MAKE
	t0=$(date +%s)
	make -C "$C/freertos/ticlang" "${MK[@]}" > "$RUN/logs/coremark-$o.txt" 2>&1 \
		&& step "coremark-$o" "ok in $(( $(date +%s) - t0 )) s: $C/freertos/ticlang/uart2echo.out" \
		|| step "coremark-$o" "FAILED, see logs/coremark-$o.txt: $(grep -m1 -i error "$RUN/logs/coremark-$o.txt" | cut -c1-160)"
done

# 3 occupancy models, Cortex-M33 objects (sizes only)
U="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"; O="$RUN/occupancy"; mkdir -p "$O"
for m in occ_cnn_500 occ_cnn_2k occ_cnn_8k occ_cnn_30k; do for o in Os O3; do
	"$CGT/bin/tiarmclang" -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mthumb -$o -ffp-contract=off -I"$U/$m" -c "$U/$m/$m.c" -o "$O/$m-$o.o" 2>> "$RUN/logs/occupancy.txt"
done; done
"$CGT/bin/tiarmsize" "$O"/*.o > "$O/sizes.txt" 2>&1; step occupancy "$(ls "$O"/*.o 2>/dev/null | wc -l | tr -d ' ') of 8 objects: $O/sizes.txt"

# 4 sweep build pass
okc=0; n=0; : > "$RUN/sweep.csv"
for k in rtos nortos; do for cat in "$SDK/examples/$k/LP_EM_CC35X1"/*/; do for e in "$cat"*/; do
	[ -d "$e" ] || continue; n=$((n+1)); name="$(basename "$cat")/$(basename "$e")"
	r=$(build_one "$e" "$RUN/sweep/$k/$name"); [ "$r" = ok ] && okc=$((okc+1)); echo "$k,$name,${r//,/;}" >> "$RUN/sweep.csv"
done; done; done
step sweep "$okc of $n examples built (TI Arm Clang): $RUN/sweep.csv"

# 5 MQTT client, placeholder network (wifi_settings.h keeps the SDK's Example_SSID; never commit a real one)
r=$(build_one "$EX/demos/mqtt_client" "$RUN/mqtt/mqtt_client"); step mqtt "$r"

# 6 border-router host libraries (needs ~/ot-ti; see border-router/README.md)
if [ -x "$HOME/ot-ti/_tools/gcc/bin/arm-none-eabi-gcc" ]; then
	bash "$REPO/border-router/host/build.sh" "$RUN/brhost" > "$RUN/logs/brhost.txt" 2>&1 && step brhost ok || step brhost "FAILED, see logs/brhost.txt"
else step brhost "skipped: ~/ot-ti toolchain not found"; fi
echo "done $(date '+%H:%M:%S'), load: $(uptime)" | tee -a "$S"
