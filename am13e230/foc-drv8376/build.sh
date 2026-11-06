#!/bin/bash
# Build TI's AM13 3-shunt Hall-sensored FOC solution, ported to the DRV8376EVM on the LP-AM13E230.
# BUILD ONLY. This script never flashes and never touches a USB device.
#   ./build.sh
# Output: out/am13e230/foc-drv8376/foc.out (outside the repo), plus foc.map and build.log.
# Same toolchain as ../bldc-drv8376/build.sh (SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS, the SDK's prebuilt libraries)
# and the flags of the SDK solution's Release configuration (example.projectspec): -O3 -ffast-math -finline-functions.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-$PWD/out/am13e230}/foc-drv8376"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
RTL="$SDK/source/realtimelib"
CF="$HERE/common_files"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
t0=$(python3 -c 'import time;print(time.time())')

# 1. SysConfig: pins, ADC, CMPSS, PWM, crossbars, UART -> ti_sdk_dl_config.[ch]
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet \
  --output "$OUT/gen" "$HERE/foc_drv8376.syscfg" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; cat "$OUT/sysconfig.log"; exit 1; }

# 2. Flags, includes, defines
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
OPT="-O3 -ffast-math -finline-functions"
WARN="-Wall -Wextra -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -Wno-unused-command-line-argument -Wno-unused-parameter"
DEFS="-DDRV8376EVM_REVA -DMOTOR_THREE_SHUNT -DDATALOGF2_EN -D_FULL_FAST_LIB -DFRL_COMMISSIONING=${FRL_COMMISSIONING:-1}"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/cmsis/DSP/Include -I$SDK/source/driverlib/am13e230x -I$SDK/source/mathlib/fastrts \
 -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source \
 -I$CF/hal/foc_interface/include -I$CF/hal/controller_interface/include -I$CF/hal/communication_interface/include \
 -I$CF/hal/gate_driver_interface/include -I$CF/hal/gate_driver_interface/drv8376 -I$CF/app_lib/include"
for d in transforms/svgen transforms/clarke transforms/park transforms/ipark transforms/volts math filter/offset filter/notch \
         filter/filter_fo filter/filter_so control/dead_time_comp control/vib_comp control/vsf control/pi control/fwc control/vs_freq \
         control/ti_tune control/pid control/dclink_ss control/ipd control/mtpa control/pr observers/esmo observers/est_lib \
         observers/flux_obs observers/speedfr utilities/types utilities/angle_gen utilities/datalog utilities/rampgen utilities/traj \
         utilities/cpu_time utilities/motorvision; do INC="$INC -I$RTL/$d/include"; done
INC="-I$HERE/motorvision_config $INC -I$HERE -I$HERE/system_config -I$OUT/gen"  # motorvision_config first: its motorvision_hash.h sizes the table

# 3. Sources: this folder, the local copy of common_files, SDK real-time library (unmodified, read in place), SysConfig output
SRCS="$HERE/commissioning.c $HERE/frl_guard.c $HERE/isr.c $HERE/main.c $HERE/system_config/system_config.c $HERE/system_config/mtr1_init.c $HERE/motorvision_config/motorvision_hash.c
 $CF/hal/controller_interface/source/hal.c $CF/app_lib/source/motor_common.c $CF/app_lib/source/ti_tune.c
 $CF/app_lib/source/hall_sen_foc_app.c $CF/app_lib/source/motor_params.c $CF/app_lib/source/hall_cal.c
 $CF/hal/communication_interface/source/motorvision_uart_hal.c $CF/hal/foc_interface/source/hal_hall_sen_interface.c
 $CF/hal/foc_interface/source/hal_3sh_foc_interface.c $CF/hal/gate_driver_interface/drv8376/drv8376_interface.c
 $RTL/utilities/motorvision/source/motorvision.c $RTL/utilities/motorvision/source/export.c $RTL/utilities/motorvision/source/export_package.c
 $RTL/transforms/svgen/source/svgen_current.c $RTL/transforms/park/source/park.c $RTL/transforms/ipark/source/ipark.c
 $RTL/transforms/volts/source/volt_recons.c $RTL/transforms/volts/source/volt_calc.c $RTL/filter/offset/source/offset.c
 $RTL/filter/notch/source/filter_notch.c $RTL/filter/filter_fo/source/filter_fo.c $RTL/filter/filter_so/source/filter_so.c
 $RTL/control/vib_comp/source/vib_comp.c $RTL/control/vsf/source/vsf.c $RTL/control/pi/source/pi.c $RTL/control/fwc/source/fwc.c
 $RTL/control/vs_freq/source/vs_freq.c $RTL/control/pid/source/pid.c $RTL/control/dclink_ss/source/dclink_ss.c
 $RTL/control/ipd/source/ipd.c $RTL/control/mtpa/source/mtpa.c $RTL/control/pr/source/pr.c $RTL/observers/esmo/source/esmo.c
 $RTL/observers/speedfr/source/speedfr.c $RTL/utilities/angle_gen/source/angle_gen.c $RTL/utilities/traj/source/traj.c
 $RTL/utilities/cpu_time/source/cpu_time.c $OUT/gen/ti_sdk_dl_config.c"

# 4. Compile, in parallel; one log per file, merged afterwards
compile_one() {
  f="$1"; o="$OUT/obj/$(echo "${f#/}" | tr '/' '_' | sed 's/\.c$/.o/')"
  # the bench guard checks for NaN and infinity: -ffast-math lets the compiler fold those checks away (a NaN speed
  # reference passed as -40 Hz in the guard suite, 2026-10-19), so the guard file keeps fast-math but not finite-math
  local x=""; case "$f" in */frl_guard.c) x="-fno-finite-math-only";; esac
  "$CC/bin/tiarmclang" $ARCH $OPT $x -g $WARN $DEFS $INC -c "$f" -o "$o" > "$o.log" 2>&1 || { echo "FAILED $f" >> "$OUT/failed.txt"; }
}
export -f compile_one; export CC ARCH OPT WARN DEFS INC OUT
rm -f "$OUT/failed.txt"
echo $SRCS | tr ' ' '\n' | grep . | xargs -P "$JOBS" -I{} bash -c 'compile_one {}'
cat "$OUT"/obj/*.log > "$OUT/build.log" 2>/dev/null
if [ -f "$OUT/failed.txt" ]; then echo "Compile failed for: $(tr '\n' ' ' < "$OUT/failed.txt")"; echo "Log: $OUT/build.log"; grep -E "error" "$OUT/build.log" | head -30; exit 1; fi
NWARN=$(grep -c "warning:" "$OUT/build.log"); echo "compiled $(echo $SRCS | wc -w | tr -d ' ') files, $NWARN warnings (log: $OUT/build.log)"

# 5. Link: SDK prebuilt driverlib, arch, CMSIS-DSP, utils, device and the real-time library archives, linker script of the SDK solution
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -Wl,-i$SDK -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibCMSISDSP_m33_ti_arm_clang.a -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -lsource/realtimelib/control/dead_time_comp/lib/dead_time_comp_lib_am13e230x_m33_ti_arm_clang.a \
 -lsource/realtimelib/control/ti_tune/lib/titune_lib_am13e230x_m33_ti_arm_clang.a \
 -lsource/realtimelib/control/ipd/lib/ipd_lib_am13e230x_m33_ti_arm_clang.a \
 -lsource/realtimelib/observers/est_lib/lib/fast_full_lib_am13e230x_m33_ti_arm_clang.a \
 -lsource/realtimelib/observers/flux_obs/lib/flux_obs_lib_am13e230x_m33_ti_arm_clang.a \
 -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/foc.map" "-L$CC/lib" -llibc.a -o "$OUT/foc.out" ) > "$OUT/link.log" 2>&1 \
  || { echo "Link failed: $OUT/link.log"; head -60 "$OUT/link.log"; exit 1; }
cat "$OUT/link.log" >> "$OUT/build.log"
t1=$(python3 -c 'import time;print(time.time())')
echo "built $OUT/foc.out in $(python3 -c "print(f'{$t1-$t0:.1f}')") s"
echo "memory: $(grep -E '^ +(FLASH_BANK0|RAM_S|RAM_C|RAM) ' "$OUT/foc.map" | awk '{print $1, "origin", $2, "length", $3, "used", $4, "unused", $5}' | tr '\n' ';')"
