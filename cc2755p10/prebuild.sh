#!/bin/bash
# Build everything for the LP-EM-CC2755P10 that needs no board, before it arrives. NOT RUN YET (written 2026-10-07).
# Touches no probe, serial port or board. Writes only under out/cc2755p10/.
#   ./prebuild.sh          refuse if the Mac is busy (1-minute load average at or above the core count)
#   ./prebuild.sh --wait   wait for a quiet Mac instead
# Steps, each logged to $RUN/logs/<step>.txt and summarised in $RUN/summary.txt:
#   1 blink   SDK nortos drivers/empty (toggles the red LED, DIO16) built with its own makefile
#   2 coremark  third-party/coremark (unmodified) + baremetal/coremark/core_portme.c at -O3 and -Oz
#   3 bench   the shared bench suite (Dhrystone, math, AES, Embench, the four occupancy models, BitNetMCU,
#             ADC rate, FOC) via bench/build.sh
#   4 sweep   build pass of every SDK example for LP_EM_CC2755P10 (tools/board_sweep), no running
#   5 thread  TI ot-ti (thread-v1.4-ti-2.1 checkout in ~/ot-ti) CLI FTD, CLI MTD, RCP and NCP FTD images
# Matter is not built here: TI's Matter release lists Ubuntu 22.04 as its only build host (see the arrival plan).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
RUN="$PWD/out/cc2755p10"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
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
for p in "$SDK" "$SYSCFG" "$CGT/bin/tiarmclang"; do [ -e "$p" ] || { echo "missing: $p (install it first)"; exit 4; }; done
mkdir -p "$RUN/logs"; S="$RUN/summary.txt"; echo "prebuild cc2755p10 $(date '+%Y-%m-%d %H:%M:%S'), load: $(uptime)" > "$S"
step() { echo "== $1: $2" | tee -a "$S"; }

# 1 blink
B="$RUN/blink/empty"; rm -rf "$B"; mkdir -p "$RUN/blink"; cp -R "$SDK/examples/nortos/LP_EM_CC2755P10/drivers/empty" "$B"
t0=$(date +%s)
if make -C "$B/ticlang" SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" > "$RUN/logs/blink.txt" 2>&1; then
	step blink "ok in $(( $(date +%s) - t0 )) s: $B/ticlang/empty.hex"
else step blink "FAILED, see logs/blink.txt: $(grep -m1 -i error "$RUN/logs/blink.txt")"; fi

# 2 CoreMark at -O3 and -Oz
for o in O3 Oz; do
	d="$RUN/coremark/build-$o"; rm -rf "$d"; mkdir -p "$d"; t0=$(date +%s)
	if make -C "$d" -f "$HERE/baremetal/makefile" SRC="$HERE/baremetal" REPO="$REPO" OPT=-$o \
		SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" > "$RUN/logs/coremark-$o.txt" 2>&1; then
		ok=1
		for p in allowDebugPort allowChipErase allowFlashProgram allowReturnToFactory; do
			grep -qE "\.$p[[:space:]]*=[[:space:]]*CCFG_PERMISSION_ALLOW" "$d/ti_devices_config.c" || { ok=0; step "coremark-$o" "CCFG $p not ALLOW: image must not be flashed"; }
		done
		[ $ok = 1 ] && step "coremark-$o" "ok in $(( $(date +%s) - t0 )) s, CCFG permissions checked: $d/bench.hex"
		awk '/MEMORY CONFIGURATION/,/SEGMENT ALLOCATION MAP/' "$d/bench.map" | grep -E "^ +(FLASH|SRAM)" >> "$S"
	else step "coremark-$o" "FAILED, see logs/coremark-$o.txt: $(grep -m1 -i error "$RUN/logs/coremark-$o.txt")"; fi
done

# 3 bench suite (the occupancy models are its ai_occ recipe)
BENCH_OUT_ROOT="$RUN/bench" "$HERE/bench/build.sh" > "$RUN/logs/bench.txt" 2>&1
step bench "$(grep -c ',yes,' "$RUN/bench/out/images.csv" 2>/dev/null) images built, $(grep -c ',no,' "$RUN/bench/out/images.csv" 2>/dev/null) failed: $RUN/bench/out/images.csv"

# 4 sweep build pass
W="$RUN/sweep"; mkdir -p "$W"
python3 "$REPO/tools/board_sweep/find_examples.py" LP_EM_CC2755P10 "$W/examples.csv" > "$RUN/logs/sweep-find.txt" 2>&1
python3 "$REPO/tools/board_sweep/build_all.py" "$W/examples.csv" "$W" "$W/builds.jsonl" > "$RUN/logs/sweep-build.txt" 2>&1
step sweep "$(grep -c '"ok": true' "$W/builds.jsonl" 2>/dev/null) of $(wc -l < "$W/builds.jsonl" 2>/dev/null) TI Arm Clang builds ok: $W/builds.jsonl"

# 5 Thread images from TI's ot-ti (Cortex-M33 toolchain file, as ot-ti's script/build does for LP_EM_CC2755P10)
OT="$HOME/ot-ti"
if [ -x "$OT/_tools/gcc/bin/arm-none-eabi-gcc" ] && [ -d "$OT/third_party/ti_simplelink_sdk/repo_cc23xx_cc27xx" ]; then
	export PATH="$OT/_tools/gcc/bin:$OT/_tools/venv/bin:$PATH"; export TI_SYSCONFIG_ROOT=/Applications/ti/sysconfig_1.26.3
	for v in default ncp; do
		bd="$RUN/thread/$v"; extra=""; [ $v = ncp ] && extra="-DNCP_BUILD_ENABLED=ON"
		( cd "$OT" && cmake -GNinja -DOT_COMPILE_WARNING_AS_ERROR=ON -DTI_SIMPLELINK_BOARD=LP_EM_CC2755P10 -DOT_PLATFORM=external \
			-DCMAKE_TOOLCHAIN_FILE=src/arm-none-eabi-m33-cc27xx.cmake $extra "$OT" -B "$bd" && cmake --build "$bd" -- -k 0 ) > "$RUN/logs/thread-$v.txt" 2>&1 \
			&& step "thread-$v" "ok: $(cd "$bd" && find . -name '*.out' -o -name 'ot-*' -type f -perm +111 | head -8 | tr '\n' ' ')" \
			|| step "thread-$v" "FAILED or partial, see logs/thread-$v.txt"
		# Keep the images and maps, drop the build tree: ot-ti's raw .bin output spans flash to the high CCFG
		# address and comes out about 1.3 GB per image (2026-10-08 the build filled the disk). Flash the .out.
		mkdir -p "$RUN/thread/images-$v" && find "$bd" \( -name 'ot-*.out' -o -name 'ot-*.map' -o -name 'ot-*.hex' \) -exec cp {} "$RUN/thread/images-$v/" \; ; rm -rf "$bd"
	done
	[ -f "$OT/_tools/mapmem.py" ] && python3 "$OT/_tools/mapmem.py" 165888 $(find "$RUN/thread" -name '*.map') >> "$S" 2>&1   # 162 kB RAM
else
	step thread "skipped: ~/ot-ti toolchain not found (setup: cc2340r5/thread/README.md, Exact commands)"
fi
echo "done $(date '+%H:%M:%S'), load: $(uptime)" | tee -a "$S"
