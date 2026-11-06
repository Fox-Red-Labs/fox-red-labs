#!/bin/bash
# Build the BOOSTXL-AUDIO app for the LP-AM13E230 from the terminal (same toolchain and flags as am13e230/blink.sh). Does not flash.
#   ./build.sh [variant]     variant: default (UART commands), autoplay (starts the tune, amplifier line low), algos (tuner, spectrum, DTMF, gate and echo; UART commands in app/algos_app.c),
#                            halloween (the show: set list, ring, boot interlock; halloween.syscfg, main_halloween.c, app/halloween_app.c, halloween/)
# Output: $OUT (default out/boosterpack-speedrun/am13-audio-<variant>)/audio.out
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
V="${1:-default}"
OUT="${OUT:-$PWD/out/boosterpack-speedrun/am13-audio-$V}"
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0; NODE="$SYSCFG_PATH/nodejs/node"
DEF=""; [ "$V" = autoplay ] && DEF="-DAUTOPLAY"
APP="$ROOT/app/audio_app.c"; EXTRA=""; if [ "$V" = algos ]; then APP="$ROOT/app/algos_app.c"; EXTRA="$ROOT/algos/*.c"; fi
SYSCFG="$HERE/audio.syscfg"; MAIN="$HERE/main.c"
if [ "$V" = halloween ]; then APP="$ROOT/app/halloween_app.c"; EXTRA="$ROOT/halloween/*.c $ROOT/algos/ba_yin.c $OUT/tracks_gen.c"; SYSCFG="$HERE/halloween.syscfg"; MAIN="$HERE/main_halloween.c"; fi
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
[ "$V" = halloween ] && python3 "$ROOT/tools/tracks2c.py" "$OUT/tracks_gen.c" > /dev/null   # no recorded tracks: 512 kB of flash is too small for them
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet --treatWarningsAsErrors \
  --output "$OUT/gen" "$SYSCFG" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; cat "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source \
 -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp -I$ROOT/core -I$ROOT/songs -I$ROOT/app -I$ROOT/algos -I$ROOT/halloween"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$MAIN" "$APP" "$ROOT"/core/*.c $EXTRA "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $DEF $ARCH -g -Wall $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $OUT/build.log"; tail -20 "$OUT/build.log"; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/audio.map" "-L$CC/lib" -llibc.a -o "$OUT/audio.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; tail -20 "$OUT/build.log"; exit 1; }
echo "built $OUT/audio.out: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0|SRAM|FLASH) ' "$OUT/audio.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
