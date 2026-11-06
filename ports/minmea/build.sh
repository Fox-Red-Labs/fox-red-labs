#!/bin/bash
# One command: bash build.sh [shim]
#   no argument: minmea exactly as published, built for the LP-MSPM0G3507; shows the one missing call (timegm)
#   shim:        the same build plus shim/timegm_shim.c; links
# minmea is cloned at a pinned commit into ./minmea on first run and never edited.
# SDK and TC default to TI's install folders on macOS; set them to use other copies.
# DEVICE picks the chip: mspm0g3507 (default) or mspm0g3519.
set -e
cd "$(dirname "$0")"
SDK=${SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}
TC=${TC:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
COMMIT=c43c9e7
DEVICE=${DEVICE:-mspm0g3507}
case $DEVICE in
  mspm0g3507) STARTUP=startup_mspm0g350x_ticlang.c; DEF=__MSPM0G3507__ ;;
  mspm0g3519) STARTUP=startup_mspm0g351x_ticlang.c; DEF=__MSPM0G3519__ ;;
  *) echo "DEVICE must be mspm0g3507 or mspm0g3519"; exit 1 ;;
esac
[ -d minmea ] || { git clone -q https://github.com/kosma/minmea.git minmea && git -C minmea checkout -q $COMMIT; }
CC="$TC/bin/tiarmclang"
O=out/$DEVICE; mkdir -p $O
SRCS="app/main.c minmea/minmea.c $SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP"
[ "$1" = shim ] && SRCS="$SRCS shim/timegm_shim.c"
$CC -O2 -mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb -Wall -D$DEF \
  -include shim/minmea_shim.h -Iminmea -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source" \
  $SRCS -o $O/minmea.out \
  -L"$SDK/source/ti/devices/msp/m0p/linker_files/ticlang" -Wl,-l$DEVICE.cmd \
  -Wl,-m,$O/minmea.map -Wl,--rom_model -L"$TC/lib" -llibc.a
"$TC/bin/tiarmsize" $O/minmea.out 2>/dev/null || true
