#!/bin/bash
# Prebuild for the LP-MSPM0C1104: everything that needs no board, built before it arrives.
# Writes only to out/mspm0c1104/ (outside the repo). Touches no probe or port.
#
#   ./prebuild.sh                 all targets below, in order
#   ./prebuild.sh blink coremark  only these
#
# Targets (the C1104 has 16 kB of flash and 1 kB of RAM, so several of these are expected to fail at link;
# the failure and its shortfall in bytes are the result, written down as does_not_fit):
#   blink      TI's gpio_toggle_output for LP_MSPM0C1104 (needs board version E3 or later, per its README)
#   coremark   EEMBC CoreMark, unmodified files in third-party/coremark, MSPM0 bare-metal core_portme from
#              mspm0l1306/bench/coremark, 24 MHz (SysConfig of gpio_toggle_output; the chip has no PLL).
#              First the standard 2,000-byte data set (expected not to fit 1 kB of RAM), then a reduced
#              400-byte set that is NOT a valid CoreMark score, only a speed comparison point
#   occupancy  the four room-occupancy models, plain C -Os and -O3 (the smallest needs 1,152 B of RAM for its
#              buffers on the G3507 builds, so all four are expected not to fit)
#   bitnet     BitNetMCU's 1k, 12k and 16-channel CNN models, -O3, 24 MHz (the 64-channel CNN needs 2,288 B of stack)
#   sweep      find and build every SDK example for LP_MSPM0C1104 (tools/board_sweep), build pass only
# No Zephyr target: no Zephyr board for the C1104, and 1 kB of RAM is below Zephyr's minimum on the L1306.
#
# Refuses to start while the Mac is busy (1-minute load average at or above the core count), so a
# timed AI run in another session is not skewed.
# Written 2026-10-07; not run yet.
set -u
ID=mspm0c1104; BOARD=LP_MSPM0C1104
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="$PWD/out/$ID"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
START=startup_mspm0c110x_ticlang.c
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
  [ -f "$OUT/gen/g24/ti_msp_dl_config.c" ] || gen "$EX/gpio_toggle_output" "$OUT/gen/g24"
  grep -h 'CPUCLK_FREQ' "$OUT/gen/g24/ti_msp_dl_config.h"
}

b_blink() {
  rm -rf "$OUT/blink"; mkdir -p "$OUT/blink"; cp -R "$EX/gpio_toggle_output" "$OUT/blink/"
  cd "$OUT/blink/gpio_toggle_output/ticlang" && make MSPM0_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" \
    TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= && size gpio_toggle_output.out
}
t_blink() { step blink b_blink; }

cm_one() { # cm_one <gen> <opt> <clk tag> <iterations> <stack> <data bytes>
  local G=$1 O=$2 T=$3 IT=$4 ST=$5 DS=$6 B="$OUT/coremark/build-$2-$3-$6B"
  rm -rf "$B"; mkdir -p "$B"; cd "$B"
  echo "#define COMPILER_FLAGS \"-$O $ARCH, bare metal, clock $T MHz\"" > flags.h
  echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\"" >> flags.h
  local CF="-$O $(CFLAGS_BASE "$G") -I$PORT -I$CM -DITERATIONS=$IT -DPERFORMANCE_RUN=1 -DHAS_FLOAT=0 -DTOTAL_DATA_SIZE=$DS -DCM_LOG_SIZE=16 -include flags.h"
  for f in core_list_join core_matrix core_state core_util; do cc $CF -c "$CM/$f.c" -o $f.o || return 1; done
  cc $CF -Dmain=coremark_main -c "$CM/core_main.c" -o core_main.o || return 1
  cc $CF -c "$PORT/core_portme.c" -o core_portme.o && cc $CF -c "$PORT/main.c" -o main.o || return 1
  cc $CF -c "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" "$ST" coremark ./*.o
}
t_coremark() { for O in O3 Oz; do step coremark-$O-24-2000B cm_one "$OUT/gen/g24" $O 24 1000 2600 2000; done
  for O in O3 O2 Os Oz; do step coremark-$O-24-400B cm_one "$OUT/gen/g24" $O 24 1000 720 400; done; }

occ_c() { # occ_c <gen> <clk tag> <model> <Os|O3>
  local G=$1 T=$2 M=$3 L=$4 B="$OUT/occupancy/c-$3-$4-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local A=model.a; [ "$L" = O3 ] && A=model_O3.a
  local CF="-O2 $(CFLAGS_BASE "$G") -I$OCC/$M -DM=$M"
  cc $CF -c "$REPO/mspm0l1306/bench/edgeai/main.c" -o main.o && cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o \
    && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 512 occupancy main.o cfg.o startup.o "$OCC/$M/$A"
}
t_occupancy() { for M in $MODELS; do for L in Os O3; do step occ-$M-c-$L-24 occ_c "$OUT/gen/g24" 24 $M $L; done; done; }

bit_one() { # bit_one <gen> <clk tag> <model>
  local G=$1 T=$2 M=$3 B="$OUT/bitnet/$3-O3-$2"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
  local CF="-O3 $(CFLAGS_BASE "$G") -I$REPO/ports/bitnetmcu -I$REPO/third-party/bitnetmcu -I$REPO/mspm0l1306/bench/common -Wno-unused-variable -DMODEL_H=\"BitNetMCU_model_$M.h\""
  cc $CF -c "$REPO/ports/bitnetmcu/mspm0/main.c" -o main.o && cc $CF -c "$G/ti_msp_dl_config.c" -o cfg.o \
    && cc $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o startup.o || return 1
  link "$G" 700 bitnet main.o cfg.o startup.o
}
t_bitnet() { for M in 1k 12k cnn_16small; do step bitnet-$M-24 bit_one "$OUT/gen/g24" 24 $M; done; }

t_sweep() {
  mkdir -p "$OUT/sweep"
  step sweep-find python3 "$REPO/tools/board_sweep/find_examples.py" "$BOARD" "$OUT/sweep/examples.csv"
  step sweep-build python3 "$REPO/tools/board_sweep/build_all.py" "$OUT/sweep/examples.csv" "$OUT/sweep/work" "$OUT/sweep/builds.jsonl" --route make
}


TARGETS="${*:-blink coremark occupancy bitnet sweep}"
log "prebuild $ID start: targets $TARGETS; load $(sysctl -n vm.loadavg)"
step gen gens
for t in $TARGETS; do "t_$t"; done
log "prebuild $ID done. Summary: grep -c '^.* ok ' $LOG; failures: grep FAIL $LOG"
