#!/bin/bash
# Build a BP-BASSENSORSMKII example with TI Arm Clang, nothing flashed.
#   [EXTRA_CFLAGS=-DBS_STREAM_STANDIN] boards/build.sh <board> <app>        board: mspm0g5187 | mspm0g3519 | mspm33c321a    app: hello | motion_stream
# Output: $OUT/<board>/<app>/<app>.out and .map (default OUT is under out/boosterpack-speedrun/bassensors).
# SysConfig output is generated from boards/<board>/bassens.syscfg into the output folder. SysConfig: FRL_SYSCONFIG if set, else the newest installed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; S="$HERE/.."; ROOT="$HERE/../../../.."
B="$1"; APP="${2:-hello}"
OUT="${OUT:-$PWD/out/boosterpack-speedrun/bassensors}"; W="$OUT/$B/$APP"; GEN="$W/gen"
mkdir -p "$GEN"
SYSCFG="${FRL_SYSCONFIG:-}"
if [ -z "$SYSCFG" ]; then for c in /Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh /Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh /Applications/ti/ccs2101/ccs/utils/sysconfig_1.28.1/sysconfig_cli.sh; do [ -x "$c" ] && SYSCFG="$c" && break; done; fi
export FRL_SYSCONFIG="$SYSCFG"
case "$B" in
  mspm0g5187)  DEV=MSPM0G518X; START=startup_mspm0g518x_ticlang.c; FAM=m0p; ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"; TPF=mspm0-cc;;
  mspm0g3519)  DEV=MSPM0G351X; START=startup_mspm0g351x_ticlang.c; FAM=m0p; ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"; TPF=mspm0-cc;;
  mspm33c321a) DEV=MSPM33C321A; START=startup_mspm33c321x_ticlang.c; FAM=m33; ARCH="-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mthumb"; TPF=mspm33-cc;;
  *) echo "unknown board $B"; exit 2;;
esac
TOOLPATHS_FAMILY=$TPF; . "$ROOT/tools/frl/toolpaths.sh"
PRODUCT="$SDK/.metadata/product.json"
[ -f "$GEN/ti_msp_dl_config.c" ] && [ ! "$HERE/$B/bassens.syscfg" -nt "$GEN/ti_msp_dl_config.c" ] || \
  "$SYSCFG" -s "$PRODUCT" -d "$DEV" --compiler ticlang -o "$GEN" "$HERE/$B/bassens.syscfg" >/dev/null
INC="-I$GEN -I$S/include -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
CF="-Os $EXTRA_CFLAGS $ARCH @$GEN/device.opt $INC -ffunction-sections -fdata-sections -gdwarf-3 -Wall -Wno-unused-function"
CF=$(eval echo "$CF")
CLANG="$CC/bin/tiarmclang"
cd "$W"; rm -f *.o
OBJS=""
for f in "$S"/src/*.c "$HERE/mspm0_common/bs_port_mspm0.c" "$S/examples/$APP/$APP.c" "$GEN/ti_msp_dl_config.c" "$SDK/source/ti/devices/msp/$FAM/startup_system_files/ticlang/$START"; do
  o="$(basename "$f" .c).o"; "$CLANG" $CF -c "$f" -o "$o"; OBJS="$OBJS $o"
done
sed -e "s/--stack_size=[0-9]*/--stack_size=1024/" "$GEN/device_linker.cmd" > linker.cmd
"$CLANG" $ARCH -Wl,-u,_c_int00 $OBJS -l"$GEN/device.cmd.genlibs" "-L$SDK/source" linker.cmd \
   -Wl,-m,"$APP.map" -Wl,--unused_section_elimination=on -Wl,--rom_model "-L$CC/lib" -llibc.a -o "$APP.out"
rm -f *.o
echo "$B $APP:"; "$CC/bin/tiarmsize" "$APP.out" | tail -1
