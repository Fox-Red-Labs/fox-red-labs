#!/bin/bash
# Build everything for the LP-MSPM33C321A that needs no board, before it arrives.
#   ./prebuild.sh            refuse to start if the Mac is busy (1-minute load at or above the core count)
#   ./prebuild.sh --wait     wait until it is quiet, then build
#   ./prebuild.sh --no-sweep skip the build pass over every SDK example (about 250 builds)
# Output: out/mspm33c321a/ (outside the repo). One log per step, a summary at the end.
#   blink/            TI's gpio_toggle_output, built by its own makefile (the first-blink image)
#   gen32/ gen160/    SysConfig output of TI's empty (SYSOSC 32 MHz) and sysctl_mclk_syspll (SYSPLL 160 MHz) examples
#   coremark-<opt>-<mhz>/coremark.out   unmodified EEMBC files (third-party/coremark) + bench/coremark port
#   occ-<model>-<opt>-<mhz>/occupancy.out   the four room-occupancy models, plain C, built for this core
#   foc-<opt>-<mhz>/foc.out   c2000-launchpad/bench/ctrl/foc.c, one step timed (variant sw: C library on the FPU)
#   sweep/            every SDK example with a ticlang makefile, built as-is; sweep/build.csv says which built
# Nothing here touches a board or installs anything. Written 2026-10-07, not yet run (a timed run was in progress).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/mspm33c321a}"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh     # SDK 1.04 release notes: validated with SysConfig 1.28.x
EX="$SDK/examples/nortos/LP_MSPM33C321A"
WAIT=0; SWEEP=1
for a in "$@"; do case $a in --wait) WAIT=1 ;; --no-sweep) SWEEP=0 ;; esac; done

# --- quiet Mac only ---
CORES=$(sysctl -n hw.ncpu)
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; do
  if [ $WAIT = 0 ]; then echo "Mac is busy (load $(sysctl -n vm.loadavg), $CORES cores). Not building. Use --wait to wait."; exit 3; fi
  sleep 30
done

# --- tools: all three are on this Mac as of 2026-10-07 ---
miss=0
[ -x "$CC/bin/tiarmclang" ] || { echo "Missing TI Arm Clang 5.1.1.LTS: it comes with CCStudio 21.0.1 (ti.com/tool/CCSTUDIO), or alone from ti.com/tool/ARM-CGT-CLANG."; miss=1; }
[ -x "$SYSCFG" ] || { echo "Missing SysConfig 1.28.0: ti.com/tool/SYSCONFIG (the SDK was validated with 1.28.x)."; miss=1; }
[ -d "$SDK" ] || { echo "Missing MSPM33 SDK 1.04.00.00: ti.com/tool/MSPM33-SDK, mspm33_sdk_1_04_00_00.app.zip (266,797 kB, no login on 2026-10-07)."; miss=1; }
[ $miss = 0 ] || exit 2
mkdir -p "$OUT"; cd "$OUT"; : > summary.txt
uptime > load-before.txt
say() { echo "$*" | tee -a "$OUT/summary.txt"; }
size_of() { grep -E '^ +(FLASH|SRAM) ' "$1" | awk '{print $1, "used", $4}' | tr '\n' ' '; }

# --- 1. blink: TI's example, its own makefile ---
rm -rf blink; cp -R "$EX/driverlib/gpio_toggle_output" blink
t0=$(date +%s)
if make -C blink/ticlang MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > blink.log 2>&1; then
  say "blink: built in $(( $(date +%s) - t0 )) s; $(size_of blink/ticlang/gpio_toggle_output.map)"
else say "blink: FAILED, see blink.log"; fi

# --- 2. SysConfig output for the two clocks (TI's own example files) ---
gen() {   # gen <dir> <example dir> <syscfg file>: generated files plus the example's linker file, stack raised
  rm -rf "$1"; mkdir -p "$1"
  "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$1" "$2/$3" > "$1.log" 2>&1 || return 1
  "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --listGeneratedFiles --listReferencedFiles --output "$1" "$2/$3" > "$1/files.txt" 2>>"$1.log"
  sed "s/--stack_size=256/--stack_size=${STACK:-8192}/" "$2/ticlang/device_linker.cmd" > "$1/device_linker.cmd"
}
gen gen32 "$EX/driverlib/empty" empty.syscfg && say "gen32: SysConfig of driverlib/empty (32 MHz)" || say "gen32: FAILED, see gen32.log"
gen gen160 "$EX/driverlib/sysctl_mclk_syspll" sysctl_mclk_syspll.syscfg && say "gen160: SysConfig of driverlib/sysctl_mclk_syspll (160 MHz)" || say "gen160: FAILED, see gen160.log"

ARCH="-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16"
common_cflags() {  # common_cflags <opt> <gen dir> <hz>
  echo "-$1 $ARCH @$2/device.opt -I$2 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall -DCPU_HZ=$3u"
}
compile() {   # compile <outdir> <c files...>   (uses $CFLAGS; objects land in <outdir>)
  local o=$1; shift; mkdir -p "$o"
  for f in "$@"; do "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$o/$(basename "${f%.c}").o" >> "$o.log" 2>&1 || return 1; done
}
gen_c() { grep '\.c$' "$1/files.txt" | sed "s|^\([^/]\)|$1/\1|"; }   # the generated and referenced .c files of a gen dir
link() {      # link <outdir> <gen dir> <name>
  ( cd "$1" && "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 ./*.o -l"$2/device.cmd.genlibs" "-L$SDK/source" "$2/device_linker.cmd" \
      -Wl,-m,"$3.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$3.out" ) >> "$1.log" 2>&1
}
build() {     # build <outdir> <gen dir> <name> <c files...>: fresh folder, compile with the board files, link
  local o=$1 g=$2 n=$3; shift 3; rm -rf "$o" "$o.log"
  compile "$o" "$@" $(gen_c "$g") && link "$o" "$g" "$n"
}

# --- 3. CoreMark: unmodified EEMBC files, about 10 s per run at each clock ---
CM="$REPO/third-party/coremark"; P="$HERE/bench/coremark"
for mhz in 32 160; do
  [ $mhz = 32 ] && ITER=${ITER32:-1300} || ITER=${ITER160:-6500}   # CoreMark needs 10 s or more: measured 9.3 s at 1300 and 32 MHz, so 2026-10-08 runs use ITER32=1600 ITER160=9000
  for opt in O3 O2 Os Oz; do
    o="$OUT/coremark-$opt-$mhz"; g="$OUT/gen$mhz"
    { echo "#define COMPILER_FLAGS \"-$opt $ARCH, bare metal, clock $mhz MHz\""
      echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\""; } > "$o.flags.h"
    base="$(common_cflags $opt $g ${mhz}000000) -I$CM -I$P -DITERATIONS=$ITER -DPERFORMANCE_RUN=1 -DCM_LOG_SIZE=2048 -DBOARD_HEADER=\"ti_msp_dl_config.h\" -include $o.flags.h"
    rm -rf "$o" "$o.log"
    CFLAGS="$base" compile "$o" "$CM/core_list_join.c" "$CM/core_matrix.c" "$CM/core_state.c" "$CM/core_util.c" "$P/core_portme.c" "$P/main.c" $(gen_c "$g") \
      && CFLAGS="$base -Dmain=coremark_main" compile "$o" "$CM/core_main.c" && link "$o" "$g" coremark \
      && say "coremark-$opt-$mhz: built; $(size_of $o/coremark.map)" || say "coremark-$opt-$mhz: FAILED, see $o.log"
  done
done

[ "${COREMARK_ONLY:-0}" = 1 ] && { uptime > load-after.txt; exit 0; }   # COREMARK_ONLY=1 OUT=<dir> ITER32=.. ITER160=.. ./prebuild.sh --no-sweep
# --- 4. room-occupancy models, plain C from the model folders, -O3 and -Os ---
MD="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"
for mhz in 160 32; do for m in occ_cnn_500 occ_cnn_2k occ_cnn_8k occ_cnn_30k; do for opt in O3 Os; do
  [ $mhz = 32 ] && [ $opt = Os ] && continue           # 32 MHz is only for the M0+ comparison at -O3
  o="occ-$m-$opt-$mhz"
  CFLAGS="$(common_cflags $opt gen$mhz ${mhz}000000) -I$MD/$m -DM=$m -DBOARD_HEADER=\"ti_msp_dl_config.h\""
  build "$OUT/$o" "$OUT/gen$mhz" occupancy "$HERE/bench/occupancy/main.c" "$MD/$m/$m.c" \
    && say "$o: built; $(size_of $OUT/$o/occupancy.map)" || say "$o: FAILED (does_not_fit if the log says the memory is full), see $o.log"
done; done; done

# --- 5. foc_step with the C2000's foc.c, -O2 as on the F2800137 and -O3 ---
for mhz in 160 32; do for opt in O2 O3; do
  o="foc-$opt-$mhz"
  CFLAGS="$(common_cflags $opt gen$mhz ${mhz}000000) -I$REPO/c2000-launchpad/bench/ctrl -DSTR_FOC_SRC=\"$REPO/c2000-launchpad/bench/ctrl/foc.c\" \
    -DBOARD_HEADER=\"ti_msp_dl_config.h\""
  build "$OUT/$o" "$OUT/gen$mhz" foc "$HERE/bench/foc/main.c" && say "$o: built; $(size_of $OUT/$o/foc.map)" || say "$o: FAILED, see $o.log"
done; done

# --- 6. sweep build pass: every example with a ticlang makefile, unchanged ---
if [ $SWEEP = 1 ]; then
  mkdir -p sweep; echo "kind,category,example,result,seconds" > sweep/build.csv
  find "$SDK/examples" -path '*LP_MSPM33C321A*' -path '*/ticlang/makefile' | sort | while read -r mk; do
    t="$(dirname "$mk")"; ex="$(dirname "$t")"; case "$ex" in */freertos) ex="$(dirname "$ex")" ;; esac
    rel="${ex#$EX/}"; rel="${rel#$SDK/examples/rtos/LP_MSPM33C321A/}"; d="sweep/$(echo "$rel" | tr '/' '_')"
    kind=nortos; case "$mk" in */rtos/*) kind=rtos ;; esac
    rm -rf "$d"; cp -R "$ex" "$d"; tt="$d/${t#$ex/}"; s=$(date +%s)
    if make -C "$tt" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$d.log" 2>&1
    then r=built; else r=failed; fi
    echo "$kind,$(dirname "$rel"),$(basename "$rel"),$r,$(( $(date +%s) - s ))" >> sweep/build.csv
  done
  say "sweep: $(grep -c ',built,' sweep/build.csv) built, $(grep -c ',failed,' sweep/build.csv) failed (sweep/build.csv)"
fi
uptime > load-after.txt
say "done: $OUT/summary.txt"
