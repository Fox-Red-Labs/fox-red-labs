#!/bin/bash
# Compile LVGL + src/app.c for the Mac, render the demo screen into out/host/screen.png, print heap use.
# Heap numbers are a 64-bit upper bound for the 32-bit chip. Usage: tools/host_check.sh [fit|full]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; V="${1:-fit}"; O="$HERE/out/host_$V"; mkdir -p "$O/obj"
DEF=""; [ "$V" = full ] && DEF="-DDEMO_FULL"
CF="-O1 $DEF -DLV_CONF_INCLUDE_SIMPLE -DLV_MEM_SIZE=$((${MEM_KB:-16}*1024)) -DDRAW_ROWS=${ROWS:-10} -I$HERE -I$HERE/lvgl -I$HERE/src -I$HERE/tools/host/inc -w"
# only the LVGL files the chip build actually links (from its map), to keep the host build small
MAP="$HERE/out/lvgl_32mhz/lvgl_32mhz.map"
( cd "$HERE/lvgl/src"; find . -name '*.c' -not -path './drivers/*' -not -path './libs/*' ) | sort | while read -r f; do
  o="lvgl_$(echo "${f#./}" | tr '/' '_' | sed 's/\.c$/.o/')"; grep -q " $o (" "$MAP" && echo "$f"; done > "$O/files.txt" || true
c1() { f="$1"; o="$O/obj/$(echo "${f#./}" | tr '/' '_' | sed 's/\.c$/.o/')"; [ -f "$o" ] || clang $CF -c "$HERE/lvgl/src/${f#./}" -o "$o"; }
export -f c1; export CF HERE O
xargs -P 6 -I{} bash -c 'c1 {}' < "$O/files.txt"
clang $CF -c "$HERE/src/app.c" -o "$O/app.o"; clang $CF -c "$HERE/tools/host/host_main.c" -o "$O/host_main.o"
clang -Wl,-dead_strip "$O"/obj/*.o "$O/app.o" "$O/host_main.o" -o "$O/host"
"$O/host" "$O/screen.ppm" | tee "$O/host.log"
python3 -c "
from PIL import Image
Image.open('$O/screen.ppm').save('$O/screen.png')"
echo "screen: $O/screen.png"
