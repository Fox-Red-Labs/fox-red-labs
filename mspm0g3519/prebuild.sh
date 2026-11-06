#!/bin/bash
# Prebuild for the LP-MSPM0G3519: everything that needs no board, built before it arrives.
# Writes only to out/mspm0g3519/ (outside the repo). Touches no probe or port.
#
#   ./prebuild.sh                 all targets below, in order
#   ./prebuild.sh blink coremark  only these
#
# Targets:
#   blink      TI's gpio_toggle_output for LP_MSPM0G3519, built by its own makefile
#   coremark   EEMBC CoreMark, unmodified files in third-party/coremark, with the MSPM0 bare-metal
#              core_portme from mspm0l1306/bench/coremark (SysTick cycle counter, results in RAM).
#              TI Arm Clang at -O3 -O2 -Os -Oz, at 80 MHz (SysConfig of TI example sysctl_mclk_syspll)
#              and 32 MHz (SysConfig of gpio_toggle_output), the clocks the G3507 rows used
#   occupancy  the four room-occupancy models: plain C archives (-Os, -O3) and TI NNC's CPU build,
#              one image per model, harnesses from mspm0l1306/bench/edgeai and mcu-bench/edgeai-sdk
#   bitnet     BitNetMCU's four handwriting models (ports/bitnetmcu), -O3, 32 and 80 MHz
#   sweep      find and build every SDK example for LP_MSPM0G3519 (tools/board_sweep), build pass only
#   zephyr     Zephyr 4.4.2 blinky and the mcu-bench app for the upstream lp_mspm0g3519 board
#
# Refuses to start while the Mac is busy (1-minute load average at or above the core count), so a
# timed AI run in another session is not skewed.
# Written 2026-10-07; not run yet.
set -u
ID=mspm0g3519; BOARD=LP_MSPM0G3519
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="$PWD/out/$ID"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
START=startup_mspm0g351x_ticlang.c
EX="$SDK/examples/nortos/$BOARD/driverlib"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
CM="$REPO/third-party/coremark"; PORT="$REPO/mspm0l1306/bench/coremark"
OCC="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"
MODELS="occ_cnn_500 occ_cnn_2k occ_cnn_8k occ_cnn_30k"

# --- the Mac must be quiet -------------------------------------------------------------------
CORES=$(sysctl -n hw.ncpu)
if [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; then
  echo "Load average is at or above $CORES cores; waiting for a quiet Mac (another session may be timing a run)."
fi
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; do sleep 30; done

# --- tools present? (nothing is installed by this script) ------------------------------------
missing=0
for p in "$SDK/examples/nortos/$BOARD" "$CC/bin/tiarmclang" "$SYSCFG"; do [ -e "$p" ] || { echo "missing: $p"; missing=1; }; done
if [ $missing = 1 ]; then
  cat <<'MSG'
Install, in this order (versions read on ti.com 2026-10-07):
  1. CCStudio 21.0.1 for macOS (https://www.ti.com/tool/download/CCSTUDIO). It brings TI Arm Clang 5.1.1.LTS
     under /Applications/ti/ccs2101/ccs/tools/compiler/.
  2. MSPM0 SDK 2.11.00.07 (https://www.ti.com/tool/download/MSPM0-SDK), file mspm0_sdk_2_11_00_07.app.zip,
     399,069 kB; TI asks for a one-minute export approval before the download.
  3. SysConfig 1.26.3 standalone (https://www.ti.com/tool/download/SYSCONFIG), to /Applications/ti/sysconfig_1.26.3.
Then run this script again.
MSG
  exit 2
fi

mkdir -p "$OUT"; LOG="$OUT/prebuild.log"; : >> "$LOG"
log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" | tee -a "$LOG"; }
now() { python3 -c 'import time; print(time.time())'; }
cc() { "$CC/bin/tiarmclang" "$@"; }
size() { [ -f "$1" ] && "$CC/bin/tiarmsize" "$1" | tail -1 | awk '{print "text="$1" data="$2" bss="$3}'; }
# step <name> <command...>: run in a subshell, log ok or fail with seconds, keep going on failure
step() { local n=$1; shift; local t0; t0=$(now); if ( "$@" ) > "$OUT/logs/$n.log" 2>&1; then
  log "ok   $n $(python3 -c "print(round($(now)-$t0,1))") s $(tail -1 "$OUT/logs/$n.log" | cut -c1-120)"
  else log "FAIL $n $(python3 -c "print(round($(now)-$t0,1))") s: $(grep -m1 -iE 'error|fatal|not found|exceed|overflow' "$OUT/logs/$n.log" | cut -c1-200)"; fi; }
mkdir -p "$OUT/logs"
# gen <SDK example folder> <output folder>: SysConfig output (ti_msp_dl_config.c/.h, device_linker.cmd,
# device.opt, device.cmd.genlibs) from the example's own .syscfg, unchanged
gen() { mkdir -p "$2"; cp "$1"/*.syscfg "$2/"; "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" \
  --output "$2" "$(ls "$2"/*.syscfg | head -1)" > "$2/sysconfig.log" 2>&1; }
# link <gen folder> <stack bytes> <name> <objects and archives...>
link() { local G=$1 ST=$2 N=$3; shift 3
  sed -E "s/--stack_size=[0-9]+/--stack_size=$ST/" "$G/device_linker.cmd" > device_linker.cmd
  cc $ARCH -Wl,-u,_c_int00 "$@" -l"$G/device.cmd.genlibs" "-L$SDK/source" ./device_linker.cmd \
    -Wl,-m,"$N.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$N.out"
  grep -E '^ +(FLASH|SRAM) ' "$N.map"; size "$N.out"; }
CFLAGS_BASE() { echo "$ARCH @$1/device.opt -I$1 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall"; }

gens() {
  [ -f "$OUT/gen/g32/ti_msp_dl_config.c" ] || gen "$EX/gpio_toggle_output" "$OUT/gen/g32"
  [ -f "$OUT/gen/g80/ti_msp_dl_config.c" ] || gen "$EX/sysctl_mclk_syspll" "$OUT/gen/g80"
  grep -h 'CPUCLK_FREQ' "$OUT/gen/g32/ti_msp_dl_config.h" "$OUT/gen/g80/ti_msp_dl_config.h"
}

b_blink() {
  rm -rf "$OUT/blink"; mkdir -p "$OUT/blink"; cp -R "$EX/gpio_toggle_output" "$OUT/blink/"
  cd "$OUT/blink/gpio_toggle_output/ticlang" && make MSPM0_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" \
    TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= && size gpio_toggle_output.out
}
t_blink() { step blink b_blink; }

cm_one() { # cm_one <gen> <opt> <clk tag> <iterations>
  local G=$1 O=$2 T=$3 IT=$4 B="$OUT/coremark/build-$2-$3"
  rm -rf "$B"; mkdir -p "$B"; cd "$B"
  echo "#define COMPILER_FLAGS \"-$O $ARCH, bare metal, clock $T MHz\"" > flags.h
  echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\"" >> flags.h
  local CF="-$O $(CFLAGS_BASE "$G") -I$PORT -I$CM -DITERATIONS=$IT -DPERFORMANCE_RUN=1 -DHAS_FLOAT=0 -include flags.h"
  for f in core_list_join core_matrix core_state core_util; do cc $CF -c "$CM/$f.c" -o $f.o || return 1; done
  cc $CF -Dmain=coremark_main -c "$CM/core_main.c" -o core_main.o || return 1
  cc $CF -c "$PORT/core_portme.c" -o core_portme.o && cc $CF -c "$PORT/main.c" -o main.o || return 1
  cc $CF -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 4096 coremark ./*.o
}
t_coremark() { for O in O3 O2 Os Oz; do step coremark-$O-80 cm_one "$OUT/gen/g80" $O 80 3000; step coremark-$O-32 cm_one "$OUT/gen/g32" $O 32 1500; done; }

occ_c() { # occ_c <gen> <clk tag> <model> <Os|O3>
  local G=$1 T=$2 M=$3 L=$4 B="$OUT/occupancy/c-$3-$4-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local A=model.a; [ "$L" = O3 ] && A=model_O3.a
  local CF="-O2 $(CFLAGS_BASE "$G") -I$OCC/$M -DM=$M"
  cc $CF -c "$REPO/mspm0l1306/bench/edgeai/main.c" -o main.o && cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o \
    && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 2048 occupancy main.o cfg.o startup.o "$OCC/$M/$A"
}
occ_nnc() { # occ_nnc <gen> <clk tag> <model>: TI NNC CPU build (nnc_g3507_cpu is generic Cortex-M0+)
  local G=$1 T=$2 M=$3 B="$OUT/occupancy/nnc-cpu-$3-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local CF="-O2 $(CFLAGS_BASE "$G") -I$OCC/$M/nnc_g3507_cpu -I$OCC/$M -DMODEL=$M"
  cc $CF -c "$REPO/mspm0g3507/mcu-bench/edgeai-sdk/occupancy-nnc/main.c" -o main.o && cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o \
    && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 2048 nnc main.o cfg.o startup.o "$OCC/$M/nnc_g3507_cpu/mod.a"
}
t_occupancy() { for M in $MODELS; do for L in Os O3; do step occ-$M-c-$L-80 occ_c "$OUT/gen/g80" 80 $M $L; done
  step occ-$M-nnc-cpu-80 occ_nnc "$OUT/gen/g80" 80 $M; done; }

bit_one() { # bit_one <gen> <clk tag> <model>
  local G=$1 T=$2 M=$3 B="$OUT/bitnet/$3-O3-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local CF="-O3 $(CFLAGS_BASE "$G") -I$REPO/ports/bitnetmcu -I$REPO/third-party/bitnetmcu -I$REPO/mspm0l1306/bench/common -Wno-unused-variable -DMODEL_H=\"BitNetMCU_model_$M.h\""
  cc $CF -c "$REPO/ports/bitnetmcu/mspm0/main.c" -o main.o && cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o \
    && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 3072 bitnet main.o cfg.o startup.o
}
t_bitnet() { for M in cnn_64 cnn_16small 12k 1k; do step bitnet-$M-32 bit_one "$OUT/gen/g32" 32 $M; done; step bitnet-cnn_64-80 bit_one "$OUT/gen/g80" 80 cnn_64; }

t_sweep() {
  mkdir -p "$OUT/sweep"
  step sweep-find python3 "$REPO/tools/board_sweep/find_examples.py" "$BOARD" "$OUT/sweep/examples.csv"
  step sweep-build python3 "$REPO/tools/board_sweep/build_all.py" "$OUT/sweep/examples.csv" "$OUT/sweep/work" "$OUT/sweep/builds.jsonl" --route make
}

t_zephyr() {
  local ZP="$HOME/zephyrproject" TC="$HOME/zephyr-toolchain"
  [ -d "$ZP/zephyr/boards/ti/lp_mspm0g3519" ] || { log "skip zephyr: no lp_mspm0g3519 in $ZP"; return; }
  ( . "$ZP/.venv/bin/activate" && export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-" && cd "$ZP/zephyr" \
    && step zephyr-blinky west build -p always -b lp_mspm0g3519 samples/basic/blinky -d "$OUT/zephyr/blinky" \
    && step zephyr-mcu-bench-Os west build -p always -b lp_mspm0g3519 "$REPO/mspm0g3507/mcu-bench" -d "$OUT/zephyr/mcu-bench-Os" -- -DBENCH_COREMARK_ITERATIONS=3000 \
    && step zephyr-mcu-bench-O2 west build -p always -b lp_mspm0g3519 "$REPO/mspm0g3507/mcu-bench" -d "$OUT/zephyr/mcu-bench-O2" -- -DEXTRA_CONF_FILE=speed.conf )
}

TARGETS="${*:-blink coremark occupancy bitnet sweep zephyr}"
log "prebuild $ID start: targets $TARGETS; load $(sysctl -n vm.loadavg)"
step gen gens
for t in $TARGETS; do "t_$t"; done
log "prebuild $ID done. Summary: grep -c '^.* ok ' $LOG; failures: grep FAIL $LOG"
