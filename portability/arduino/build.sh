#!/bin/bash
# Build the Arduino compatibility layer, the libraries and the sketches for one TI LaunchPad with
# TI Arm Clang. Output: out/<board>/<sketch>/<sketch>.out and .map. Does not flash anything.
#   ./build.sh                       build everything for the default board (mspm0g3507)
#   BOARD=mspm0g3507 ./build.sh      pick a board folder under boards/ (pin map, SysConfig, generated files)
#   OPT=-Os ./build.sh               change the optimization level (default -O2)
# The core/ layer is board-independent; everything a board changes lives in boards/<board>/.
# The unmodified Arduino libraries are shared by every board in ../../third-party/arduino.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# The SDK and TI Arm Clang are found the way frl finds them (TI's default folders on macOS, Linux and
# Windows; FRL_MSPM0_SDK and FRL_TICLANG override); SysConfig only when gen/ has to be made again.
. "$HERE/../../tools/frl/toolpaths.sh"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
OPT="${OPT:--O2}"
BOARD="${BOARD:-mspm0g3507}"; BD="$HERE/boards/$BOARD"; [ -d "$BD" ] || { echo "no boards/$BOARD"; exit 1; }
LIB="$HERE/../../third-party/arduino"
GEN="$BD/gen"
OUT="$HERE/out/$BOARD"; OBJ="$OUT/obj"; LOG="$OUT/logs"; mkdir -p "$OBJ" "$LOG"

# 1. SysConfig output (clock, SPI1, UART0). Skipped when gen/ is newer than the .syscfg.
if [ ! -f "$GEN/ti_msp_dl_config.c" ] || [ "$BD/arduino.syscfg" -nt "$GEN/ti_msp_dl_config.c" ]; then
  SYSCFG=""
  while IFS= read -r -d '' a; do case "$a" in SYSCONFIG_TOOL=*) SYSCFG=${a#*=};; esac; done \
    < <(python3 "$HERE/../../tools/frl/toolenv.py" sysconfig)
  [ -n "$SYSCFG" ] || exit 2
  "$SYSCFG" -s "$SDK/.metadata/product.json" -d MSPM0G350X --compiler ticlang -o "$GEN" "$BD/arduino.syscfg" >/dev/null
fi

# 2. Linker command file: SysConfig's, with a 2 KB stack (it asks for 512 bytes) and a 4 KB heap (none by default)
sed -e 's/--stack_size=512/--stack_size=2048\n--heap_size=4096/' "$GEN/device_linker.cmd" > "$OUT/linker.cmd"

INC="-I$HERE/core -I$BD -I$LIB/ArduinoCore-API -I$LIB/ArduinoCore-API/api/deprecated-avr-comp -I$LIB/ArduinoCore-API/api/deprecated -I$GEN \
 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source \
 -I$LIB/Arduino_GFX/src -I$LIB/Adafruit_BusIO -I$LIB/Adafruit_FT6206_Library"
COMMON="$OPT $ARCH @$GEN/device.opt -gdwarf-3 -Wall -Wno-unused-parameter -Wno-main -Wno-overloaded-virtual -Wno-inconsistent-missing-override -DHOST -ffunction-sections -fdata-sections $INC"
CXXF="$COMMON -std=gnu++17 -fno-exceptions -fno-rtti"
CF="$COMMON -std=gnu11"

compile() {   # compile <group> <source> [extra flags]  -> out/obj/<group>/<name>.o, only if stale
  local g="$1" s="$2"; shift 2
  local o="$OBJ/$g/$(basename "$s").o"; mkdir -p "$OBJ/$g"
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$FORCE" ]; then
    local lg="$LOG/$g-$(basename "$s").log"
    case "$s" in
      *.cpp) "$CC/bin/tiarmclang" $CXXF "$@" -c "$s" -o "$o" 2> "$lg" || { cat "$lg"; return 1; } ;;
      *.c)   "$CC/bin/tiarmclang" $CF   "$@" -c "$s" -o "$o" 2> "$lg" || { cat "$lg"; return 1; } ;;
    esac
  fi
}
group() {     # group <name> <files...>
  local g="$1"; shift
  for f in "$@"; do compile "$g" "$f"; done
}
GFX=$(ls "$LIB"/Arduino_GFX/src/*.cpp "$LIB"/Arduino_GFX/src/databus/*.cpp "$LIB"/Arduino_GFX/src/display/*.cpp "$LIB"/Arduino_GFX/src/canvas/*.cpp)

group shim   "$HERE"/core/*.cpp "$GEN/ti_msp_dl_config.c"
compile shim "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c"
group api    "$LIB"/ArduinoCore-API/api/{Common,Print,Stream,String,IPAddress}.cpp
group gfx    $GFX
group busio  "$LIB"/Adafruit_BusIO/*.cpp
group ft6206 "$LIB"/Adafruit_FT6206_Library/Adafruit_FT6206.cpp

# 3. Sketches
build_sketch() {   # build_sketch <name> <sketch dir> <main .ino> <library groups> [extra defines]
  local name="$1" dir="$2" ino="$3" groups="$4"; shift 4
  local O="$OUT/$name"; mkdir -p "$O/obj"
  CXX_CMD="$CC/bin/tiarmclang $CXXF -I$dir $*" python3 "$HERE/tools/ino2cpp.py" "$dir/$ino" "$O/$name.cpp"
  "$CC/bin/tiarmclang" $CXXF -I"$dir" "$@" -c "$O/$name.cpp" -o "$O/obj/sketch.o"
  local objs="$O/obj/sketch.o"
  for g in shim api $groups; do objs="$objs $(ls "$OBJ/$g"/*.o)"; done
  "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 $objs -l"$GEN/device.cmd.genlibs" "-L$SDK/source" "$OUT/linker.cmd" \
     -Wl,-m,"$O/$name.map" -Wl,--rom_model "-L$CC/lib" -llibc++.a -llibc++abi.a -llibc.a -llibclang_rt.builtins.a -o "$O/$name.out"
  echo "== $name"; "$CC/bin/tiarmsize" "$O/$name.out" | tail -1
}
build_sketch pdqgfx    "$HERE/sketches/pdqgfx"    PDQgraphicstest.ino "gfx"
build_sketch shimcheck  "$HERE/sketches/shimcheck"  shimcheck.ino      ""
# same sketch with the library's own SPI mode choice (mode 2 for unknown boards), to test whether it matters
build_sketch pdqgfx_libmode "$HERE/sketches/pdqgfx" PDQgraphicstest.ino "gfx" -DGFX_SPI_MODE_LIB_DEFAULT
build_sketch touchpaint "$HERE/sketches/touchpaint" CapTouchPaint.ino  "gfx busio ft6206"

# 4. Where the bytes went (shim vs libraries)
for n in shimcheck pdqgfx pdqgfx_libmode touchpaint; do echo "== $n by group"; python3 "$HERE/tools/size_by_group.py" "$OUT/$n/$n.map"; done
echo "warnings in library and shim compiles: $(cat "$LOG"/*.log 2>/dev/null | grep -c 'warning:')"
# Arduino's own built-in examples, byte-identical (zephyr/sketches/, from github.com/arduino/arduino-examples).
# Blink builds. AnalogReadSerial calls analogRead(), which this layer does not have, so it fails to compile; that failure
# is the point of the comparison with the Zephyr-based core (zephyr/README.md), so it must not stop the build.
build_sketch Blink "$HERE/zephyr/sketches/Blink" Blink.ino ""
build_sketch AnalogReadSerial "$HERE/zephyr/sketches/AnalogReadSerial" AnalogReadSerial.ino "" || echo "AnalogReadSerial: build failed (analogRead is not in the layer)"
