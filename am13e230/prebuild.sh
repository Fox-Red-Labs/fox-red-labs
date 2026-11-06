#!/bin/bash
# Build everything for the LP-AM13E230 that needs no board, before it arrives.
#   ./prebuild.sh            refuse to start if the Mac is busy (1-minute load at or above the core count)
#   ./prebuild.sh --wait     wait until it is quiet, then build
#   ./prebuild.sh --no-sweep skip the SDK's own CMake build of all examples
# Output: out/am13e230/ (outside the repo). One log per step, summary.txt at the end.
#   gen-<example>/            SysConfig output of a TI example's example.syscfg (the SDK's own CLI call, see below)
#   blink/blink.out           TI's driverlib/gpio/gpio_toggle_output (LED4 red on PA5), hand-built
#   coremark-ti-<opt>/        TI's own CoreMark port (benchmarks/coremark): TI's core_portme, 200 MHz
#   coremark-<opt>/           unmodified EEMBC files (third-party/coremark) + bench/coremark port, same clock
#   aci-O3/                   TI's ACI FOC signal-chain benchmark (benchmarks/aci), TMU on
#   foc-<sw|tmu>-<opt>/       c2000-launchpad/bench/ctrl/foc.c, one step timed; tmu = the SDK's fastrts_tmu.h remap
#   occ-<model>-<opt>/        the four room-occupancy models, plain C (the NPU builds come from TI's NNC, see the plan)
#   sdk-build/                every example through the SDK's CMake + Ninja (needs cmake; prints how to get it if missing)
# Nothing here touches a board or installs anything. Written 2026-10-07, not yet run (a timed run was in progress).
#
# Why hand-built and not the SDK makefile: the SDK's top makefile treats macOS as Linux and looks for CCStudio in
# ~/ti/ccs2100 and SysConfig in ~/ti/sysconfig_1.28.0 (make/host_os.mk, imports_am13e230x.mk); on this Mac they are in
# /Applications/ti. The paths are passed here instead. The hand builds use the SDK's prebuilt libraries in
# build/am13e230x/lib/Release and the compiler flags of the example projectspecs.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS      # the SDK asks for ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0                                # the SDK asks for sysconfig_1.28.0
NODE="$SYSCFG_PATH/nodejs/node"
CMAKE="${CMAKE:-$(command -v cmake || echo "$HOME/.venvs/cmake/bin/cmake")}"
WAIT=0; SWEEP=1
for a in "$@"; do case $a in --wait) WAIT=1 ;; --no-sweep) SWEEP=0 ;; esac; done

# --- quiet Mac only ---
CORES=$(sysctl -n hw.ncpu)
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; do
  if [ $WAIT = 0 ]; then echo "Mac is busy (load $(sysctl -n vm.loadavg), $CORES cores). Not building. Use --wait to wait."; exit 3; fi
  sleep 30
done

miss=0
[ -x "$CC/bin/tiarmclang" ] || { echo "Missing TI Arm Clang 5.1.1.LTS (comes with CCStudio 21.0.1, ti.com/tool/CCSTUDIO)."; miss=1; }
[ -x "$NODE" ] && [ -f "$SYSCFG_PATH/dist/cli.js" ] || { echo "Missing SysConfig 1.28.0 (ti.com/tool/SYSCONFIG)."; miss=1; }
[ -d "$SDK/build/am13e230x/lib/Release" ] || { echo "Missing AM13E2x SDK 26.01.00.03.STS: ti.com/tool/AM13E2X-SDK, am13e230x_sdk_26_01_00_03_STS-osx-installer.dmg (39,705 kB, read 2026-10-07)."; miss=1; }
[ $miss = 0 ] || exit 2
mkdir -p "$OUT"; cd "$OUT"; : > summary.txt; uptime > load-before.txt
say() { echo "$*" | tee -a "$OUT/summary.txt"; }
size_of() { grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|FLASH_BANK1) ' "$1" | awk '{print $1, "used", $4}' | tr '\n' ' '; }

# --- SysConfig: the exact call cmake/ti_sdk_run_sysconfig.cmake makes ---
gen() {  # gen <name> <example.syscfg>
  rm -rf "gen-$1"; mkdir -p "gen-$1"
  "$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors \
    --output "gen-$1" "$2" > "gen-$1.log" 2>&1
}
LP=am13e230x_lp/m33_nortos
gen gpio "$SDK/examples/driverlib/gpio/gpio_toggle_output/$LP/example.syscfg" && say "gen-gpio: ok" || say "gen-gpio: FAILED, see gen-gpio.log"
gen coremark "$SDK/examples/benchmarks/coremark/$LP/example.syscfg" && say "gen-coremark: ok (TI's 200 MHz clock)" || say "gen-coremark: FAILED"
gen aci "$SDK/examples/benchmarks/aci/$LP/example.syscfg" && say "gen-aci: ok" || say "gen-aci: FAILED"

ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"   # from the projectspecs
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg \
 -I$SDK/source/mathlib/fastrts -I$SDK/source/cmsis/DSP/Include -I$SDK/source"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
LINKCMD="$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd"
compile() { local o=$1; shift; mkdir -p "$o"
  for f in "$@"; do "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$o/$(basename "${f%.c}").o" >> "$o.log" 2>&1 || return 1; done; }
link() { ( cd "$1" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$LINKCMD" -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 \
  -Wl,-m,"$2.map" "-L$CC/lib" -llibc.a -o "$2.out" ) >> "$1.log" 2>&1; }
build() { local o=$1 g=$2 n=$3; shift 3; rm -rf "$o" "$o.log"; compile "$o" "$@" "$g"/*.c && link "$o" "$n"; }
base() { echo "-$1 $ARCH -g -Wall $INC -I$2 -DCPU_HZ=200000000u"; }    # 200 MHz: TI's CoreMark and ACI READMEs

# --- 1. blink ---
CFLAGS="$(base O2 gen-gpio) -I$SDK/examples/empty/am13e230x_lp" build blink gen-gpio blink "$SDK/examples/driverlib/gpio/gpio_toggle_output/main.c" \
  && say "blink: built; $(size_of blink/blink.map)" || say "blink: FAILED, see blink.log"

# --- 2. CoreMark two ways: TI's port, and the unmodified files with our port ---
TC="$SDK/examples/benchmarks/coremark"; CM="$REPO/third-party/coremark"; P="$HERE/bench/coremark"
for opt in O3 O2 Os Oz; do
  CFLAGS="$(base $opt gen-coremark) -I$TC -I$TC/am13e230x_lp/m33_nortos -I$SDK/examples/empty/am13e230x_lp -DITERATIONS=9000" build "coremark-ti-$opt" gen-coremark coremark \
    "$TC/core_list_join.c" "$TC/core_main.c" "$TC/core_matrix.c" "$TC/core_portme.c" "$TC/core_state.c" "$TC/core_util.c" \
    && say "coremark-ti-$opt: built (TI's port)" || say "coremark-ti-$opt: FAILED, see coremark-ti-$opt.log"
  o="coremark-$opt"; rm -rf "$o" "$o.log"
  { echo "#define COMPILER_FLAGS \"-$opt $ARCH, bare metal, 200 MHz\""; echo "#define COMPILER_VERSION \"TI Arm Clang 5.1.1.LTS\""; } > "$o.flags.h"
  b="$(base $opt gen-coremark) -I$CM -I$P -DITERATIONS=9000 -DPERFORMANCE_RUN=1 -DBOARD_HEADER=\"ti_sdk_dl_config.h\" -include $OUT/$o.flags.h"
  CFLAGS="$b" compile "$o" "$CM/core_list_join.c" "$CM/core_matrix.c" "$CM/core_state.c" "$CM/core_util.c" "$P/core_portme.c" "$P/main.c" gen-coremark/*.c \
    && CFLAGS="$b -Dmain=coremark_main" compile "$o" "$CM/core_main.c" && link "$o" coremark \
    && say "$o: built; $(size_of $o/coremark.map)" || say "$o: FAILED, see $o.log"
done
# Clock check on the board: both report the cycle count; time it against the Mac clock once (methodology section 11).

# --- 3. TI's ACI FOC benchmark (the C2000-style signal chain, TMU on) ---
A="$SDK/examples/benchmarks/aci"
CFLAGS="$(base O3 gen-aci) -I$A -I$A/$LP" build aci-O3 gen-aci aci "$A/aci_main.c" "$A/aci_const.c" "$A/aci_fe_const.c" "$A/aci_se_const.c" \
  && say "aci-O3: built" || say "aci-O3: FAILED (it also links CMSIS-DSP; add -llibCMSISDSP_m33_ti_arm_clang.a if the log asks), see aci-O3.log"

# --- 4. foc_step with the C2000's foc.c: sw (C library on the FPU) and tmu (fastrts_tmu.h remap) ---
for v in sw tmu; do for opt in O2 O3; do
  x=""; [ $v = tmu ] && x="-DUSE_TMU"
  CFLAGS="$(base $opt gen-coremark) $x -I$REPO/c2000-launchpad/bench/ctrl -DSTR_FOC_SRC=\"$REPO/c2000-launchpad/bench/ctrl/foc.c\" \
    -DBOARD_HEADER=\"ti_sdk_dl_config.h\" -DAM13_DEVICE_INIT" \
    build "foc-$v-$opt" gen-coremark foc "$HERE/bench/foc/main.c" && say "foc-$v-$opt: built; $(size_of foc-$v-$opt/foc.map)" || say "foc-$v-$opt: FAILED, see foc-$v-$opt.log"
done; done

# --- 5. room-occupancy models, plain C ---
MD="$REPO/mspm0g3507/mcu-bench/edgeai/uci-room-occupancy"
for m in occ_cnn_500 occ_cnn_2k occ_cnn_8k occ_cnn_30k; do for opt in O3 Os; do
  CFLAGS="$(base $opt gen-coremark) -I$MD/$m -DM=$m -DBOARD_HEADER=\"ti_sdk_dl_config.h\" -DAM13_DEVICE_INIT" \
    build "occ-$m-$opt" gen-coremark occupancy "$HERE/bench/occupancy/main.c" "$MD/$m/$m.c" \
    && say "occ-$m-$opt: built; $(size_of occ-$m-$opt/occupancy.map)" || say "occ-$m-$opt: FAILED (does_not_fit if the log says memory is full), see occ-$m-$opt.log"
done; done

# --- 6. sweep build pass: the SDK's own CMake build of every example, output kept out of /Applications ---
if [ $SWEEP = 1 ]; then
  if [ ! -x "$CMAKE" ]; then
    say "sdk-build: SKIPPED, no cmake. Homebrew has no Intel-Mac bottle for cmake 4.4.4 (it would compile from source)."
    say "  Get it as a wheel instead (PyPI cmake 4.4.4, universal2, 51.9 MB, read 2026-10-07):"
    say "  python3 -m venv ~/.venvs/cmake && ~/.venvs/cmake/bin/pip install cmake==4.4.4"
    say "  Ninja is already in /usr/local/bin. Then rerun ./prebuild.sh"
  else
    B="$OUT/sdk-build/release"; rm -rf "$OUT/sdk-build"
    "$CMAKE" -S "$SDK" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DTI_SDK_DEVICE=am13e230x -DTI_SDK_KW_BUILD=OFF -DTI_SDK_CODE_COVERAGE=OFF \
      -DTI_SDK_CPU=m33 -DTI_SDK_CPU_FEATURE=+cdecp0 -DTI_SDK_CPU_FPU_SUPPORT=ON -DTI_SDK_CGT=ti_arm_clang -DTOOLCHAIN_PATH="$CC" \
      -DTI_SDK_CMAKE_BUILD_EXE_TARGETS=ON -DSYSCFG_NODE="$NODE" -DSYSCFG_PATH="$SYSCFG_PATH" \
      -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/toolchain/toolchain_m33_ti_arm_clang.cmake" > sdk-build.log 2>&1 \
      && "$CMAKE" --build "$B" -- -k 0 >> sdk-build.log 2>&1
    say "sdk-build: $(find "$B" -name '*.out' -o -name '*.elf' | grep -c _lp) LaunchPad images built (sdk-build.log; 100 LaunchPad examples in the SDK)"
  fi
fi
uptime > load-after.txt
say "done: $OUT/summary.txt"
