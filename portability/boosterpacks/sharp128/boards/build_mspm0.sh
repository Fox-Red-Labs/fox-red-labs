#!/bin/bash
# Build a Sharp128 example for an MSPM0 LaunchPad with TI Arm Clang, nothing flashed.
#   boards/build_mspm0.sh <board> <app> [extra cflags]
#   board: mspm0g3519 | mspm0g3507 | mspm0l1306 | mspm0c1104
#   app:   hello | player (player needs a card slot: g3519 and g3507 only)
# Output: $OUT/<board>/<app>/<app>.out and .map (default OUT is the speed-run folder under out/).
# The SysConfig output is generated from boards/<board>/sharp128.syscfg into the output folder.
# SysConfig: FRL_SYSCONFIG if set, else the newest installed (the 1.26.3 the repo validated is not installed here).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; S="$HERE/.."; ROOT="$HERE/../../../.."
B="$1"; APP="${2:-hello}"; shift 2 || true
EXTRA="$*"
OUT="${OUT:-$PWD/out/boosterpack-speedrun/sharp128}"; W="$OUT/$B/$APP"; GEN="$W/gen"; STG="$W/stage"
mkdir -p "$GEN" "$STG"
SYSCFG="${FRL_SYSCONFIG:-}"
if [ -z "$SYSCFG" ]; then for c in /Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh /Applications/ti/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh; do [ -x "$c" ] && SYSCFG="$c" && break; done; fi
export FRL_SYSCONFIG="$SYSCFG"      # toolenv accepts a SysConfig other than the validated 1.26.3 when told
. "$ROOT/tools/frl/toolpaths.sh"
case "$B" in
  mspm0g3519) DEV=MSPM0G351X; START=startup_mspm0g351x_ticlang.c; BN="LP-MSPM0G3519"; SD=1;;
  mspm0g3507) DEV=MSPM0G350X; START=startup_mspm0g350x_ticlang.c; BN="LP-MSPM0G3507"; SD=1;;
  mspm0l1306) DEV=MSPM0L130X; START=startup_mspm0l130x_ticlang.c; BN="LP-MSPM0L1306"; SD=0;;
  mspm0c1104) DEV=MSPM0C110X; START=startup_mspm0c110x_ticlang.c; BN="LP-MSPM0C1104"; SD=2;;   # 2 = Petit FatFs
  *) echo "unknown board $B"; exit 2;;
esac
[ -f "$GEN/ti_msp_dl_config.c" ] && [ ! "$HERE/$B/sharp128.syscfg" -nt "$GEN/ti_msp_dl_config.c" ] || \
  "$SYSCFG" -s "$SDK/.metadata/product.json" -d "$DEV" --compiler ticlang -o "$GEN" "$HERE/$B/sharp128.syscfg" >/dev/null
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
FLAGS=""
[ "$SD" = 0 ] && FLAGS="-DSHARP_NO_SD"
[ "$SD" != 0 ] && FLAGS="-DSHARP_PORT_HAS_SD"
[ "$B" = mspm0c1104 ] && FLAGS="$FLAGS -DSHARP_NO_SD_DET -DSHARP_TIMER_TIMG14"
# The 1 kB host draws in 1-row bands; every other host keeps a 2 kB frame buffer.
[ "$B" = mspm0c1104 ] && FLAGS="$FLAGS -DSHARP_BAND_ROWS=${BAND_ROWS:-1}"
INC="-I$GEN -I$S -I$HERE/mspm0_common -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
if [ "$SD" = 1 ]; then
  # FatFs stays unmodified in third-party/fatfs; only ffconf.h is ours, so stage the sources beside it.
  for f in ff.c ff.h diskio.h ffunicode.c; do cp "$ROOT/third-party/fatfs/$f" "$STG/"; done
  cp "$S/ffconf.h" "$STG/"
  INC="$INC -I$STG"; FLAGS="$FLAGS -DSHARP_WITH_FATFS"
fi
if [ "$SD" = 2 ]; then
  for f in pff.c pff.h diskio.h; do cp "$ROOT/third-party/pff/$f" "$STG/"; done
  cp "$S/pffconf.h" "$STG/"
  INC="$INC -I$STG"; FLAGS="$FLAGS -DSHARP_WITH_PFF"
fi
CF="-Os $ARCH @$GEN/device.opt $INC -ffunction-sections -fdata-sections -gdwarf-3 -Wall -Wno-unused-function -DBOARD_NAME=\\\"$BN\\\" $FLAGS $EXTRA"
CF=$(eval echo "$CF")
CLANG="$CC/bin/tiarmclang"
cd "$W"; rm -f *.o
case "$APP" in
  hello)  MAIN="$S/examples/hello/hello.c";;
  player) MAIN="$S/player/player.c";;
  *) echo "unknown app $APP"; exit 2;;
esac
OBJS="main.o gfx.o lcd.o port.o cfg.o st.o"
"$CLANG" $CF -c "$MAIN" -o main.o
"$CLANG" $CF -c "$S/sharp_gfx.c" -o gfx.o
"$CLANG" $CF -c "$S/sharp_lcd.c" -o lcd.o
"$CLANG" $CF -c "$HERE/mspm0_common/port_mspm0.c" -o port.o
"$CLANG" $CF -c "$GEN/ti_msp_dl_config.c" -o cfg.o
"$CLANG" $CF -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$START" -o st.o
if [ "$SD" = 1 ]; then
  "$CLANG" $CF -c "$S/sharp_sd.c" -o sd.o; "$CLANG" $CF -c "$STG/ff.c" -o ff.o; "$CLANG" $CF -c "$STG/ffunicode.c" -o ffu.o
  OBJS="$OBJS sd.o ff.o ffu.o"
fi
if [ "$SD" = 2 ]; then
  "$CLANG" $CF -c "$S/sharp_sd.c" -o sd.o; "$CLANG" $CF -c "$STG/pff.c" -o pff.o
  OBJS="$OBJS sd.o pff.o"
fi
# A 64-byte default stack does not fit the band renderer: give the 1 kB host 512 bytes (the code used 352 of them on the bench).
sed -e "s/--stack_size=[0-9]*/--stack_size=${STACK:-$([ "$B" = mspm0c1104 ] && echo 512 || ([ "$B" = mspm0l1306 ] && echo 1024 || echo 2048))}/" "$GEN/device_linker.cmd" > linker.cmd
"$CLANG" $ARCH -Wl,-u,_c_int00 $OBJS -l"$GEN/device.cmd.genlibs" "-L$SDK/source" linker.cmd \
   -Wl,-m,"$APP.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$APP.out"
rm -f *.o
echo "$B $APP:"; "$CC/bin/tiarmsize" "$APP.out" | tail -1
