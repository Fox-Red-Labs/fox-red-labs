#!/bin/bash
# Builds the third-party benchmark suite for the LP-MSPM0L1306 (Cortex-M0+, 32 MHz, 64 KB flash, 4 KB SRAM)
# with TI Arm Clang 5.1.1. BUILD ONLY: never touches a probe, a serial port or the board.
# Reuses the G3507 suite's harness (common/tb.h, common/tb.c) and vendor sources by path; nothing there
# is edited. Differences are listed in README.md.
#   ./build.sh                  build everything
#   ./build.sh crc embench_st   only images whose name contains one of the words
# Per image: link once with a 256 byte stack to measure static RAM; if flash or RAM overflow, relink into a
# huge memory map to get the real numbers (image does not fit, recorded in out/nofit.txt); otherwise set the
# stack and relink. Outputs: out/<image>.out and .map, out/sizes.txt, out/nofit.txt, images.txt.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ORIG=$HOME/tmp/fox-red-labs/mspm0g3507/third-party-bench
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
GEN="$HERE/syscfg"; V="$ORIG/vendor"; OUT="$HERE/out"; B="$HERE/build-tmp"
BENCH_TIMER_DIR=$HOME/tmp/fox-red-labs/mspm0g3507/mcu-bench/sdk-common
CLANG="$CC/bin/tiarmclang"; SIZE="$CC/bin/tiarmsize"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
FLASH_LEN=65528     # 0xFFF8, the linker region SysConfig sets (the last 8 bytes of 64 KB are not used)
RAM_LEN=4096
MIN_STACK=256       # an image with less than this left for the stack is called "does not fit"
HEADROOM=64         # RAM left over when the stack depth is known
[ -d "$V/embench-iot" ] || "$ORIG/fetch.sh"
mkdir -p "$OUT"
FILTER="$*"
want() { [ -z "$FILTER" ] && return 0; for w in $FILTER; do case "$1" in *$w*) return 0;; esac; done; return 1; }

# cc <opt> <objdir> <extra flags...> -- <src>
cc() {
  local opt=$1 od=$2; shift 2
  local extra=() src w=()
  while [ "$1" != "--" ]; do extra+=("$1"); shift; done; shift; src=$1
  case "$src" in "$V"/*) w=(-w);; esac
  "$CLANG" -$opt $ARCH @"$GEN/device.opt" -I"$GEN" -I"$SDK/source/third_party/CMSIS/Core/Include" -I"$SDK/source" \
     -I"$ORIG/common" -I"$ORIG/data" -I"$BENCH_TIMER_DIR" -gdwarf-3 -Wall -Wno-unused-function "${w[@]}" "${extra[@]}" \
     -c "$src" -o "$od/$(objname "$src")"
}
objname() { echo "$1" | sed "s|^$HERE/||; s|^$ORIG/||; s|^$V/||; s|^$SDK/source/||; s|/|_|g; s|\.[a-z]*$|.o|"; }

# common objects. tb.c is the original with one include line changed (it points at bench_timer.h by a
# relative path that only works inside the G3507 tree); built from a sed copy in the scratch dir.
# nres = number of result lines the image prints (sizes tb_res, which is 1.1 KB at the original 40 entries).
common() {
  local opt=$1 od=$2 nres=$3
  sed 's|"../../mcu-bench/sdk-common/bench_timer.h"|"bench_timer.h"|' "$ORIG/common/tb.c" > "$od/tb_l1306.c"
  cc "$opt" "$od" -DTB_MAX_RESULTS=$nres -- "$od/tb_l1306.c"
  cc "$opt" "$od" -- "$GEN/ti_msp_dl_config.c"
  cc "$opt" "$od" -- "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0l130x_ticlang.c"
}
newdir() { rm -rf "$B/$1"; mkdir -p "$B/$1"; echo "$B/$1"; }

# link_try <objdir> <out.out> <map> <stack> <flash_len> <ram_len> ; returns linker status
link_try() {
  local od=$1 out=$2 map=$3 stack=$4 fl=$5 rl=$6
  sed -e "s/--stack_size=256/--stack_size=$stack/" -e "s/length = 0x0000FFF8/length = $fl/" \
      -e "/SRAM /s/length = 0x00001000/length = $rl/" "$GEN/device_linker.cmd" > "$od/lk.cmd"
  ( cd "$od" && "$CLANG" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" "$od/lk.cmd" \
      -Wl,-m,"$map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o "$out" ) >"$od/link.log" 2>&1
}
# sec <out> <section> : size of an output section from tiarmsize -A (0 if absent)
sec() { "$SIZE" -A "$1" | awk -v s="$2" '$1==s{print $2; f=1} END{if(!f) print 0}'; }

# link <image> <objdir> <stack|rest|N>
link() {
  local img=$1 od=$2 mode=$3 out="$OUT/$1.out" probe="$od/probe.out"
  # 1. probe link into a huge memory map: real flash and static RAM numbers, fit or not
  link_try "$od" "$probe" "$od/probe.map" $MIN_STACK 0x00100000 0x00100000
  local text data bss flash ram
  text=$(( $(sec "$probe" .text) + $(sec "$probe" .const) + $(sec "$probe" .cinit) + $(sec "$probe" .rodata) + $(sec "$probe" .intvecs) + $(sec "$probe" .init_array) + $(sec "$probe" .ARM.exidx) + $(sec "$probe" .binit) ))
  data=$(sec "$probe" .data); bss=$(( $(sec "$probe" .bss) + $(sec "$probe" .sysmem) + $(sec "$probe" .args) + $(sec "$probe" .vtable) ))
  # flash = everything loaded to flash (code, const, and the initial copy of .data)
  flash=$(( text + data )); ram=$(( data + bss ))
  local why=""
  [ $flash -gt $FLASH_LEN ] && why="flash over by $((flash - FLASH_LEN)) bytes ($flash used, region $FLASH_LEN)"
  [ $((ram + MIN_STACK)) -gt $RAM_LEN ] && why="$why${why:+; }RAM over by $((ram + MIN_STACK - RAM_LEN)) bytes (static $ram + $MIN_STACK min stack, of $RAM_LEN)"
  if [ -n "$why" ]; then
    printf "%-34s NOFIT  flash=%d static_ram=%d  %s\n" "$img" "$flash" "$ram" "$why" | tee -a "$OUT/nofit.txt" >> "$OUT/sizes.txt"
    printf "%-34s does not fit: %s\n" "$img" "$why"; return 0
  fi
  # 2. stack: measured depth if given, else all remaining RAM
  local free=$(( RAM_LEN - ram )) stack note
  free=$(( free / 8 * 8 ))
  if [ "$mode" = rest ]; then stack=$free; note="all remaining RAM (depth unknown)"
  else stack=$mode; note="measured depth + margin"
       [ $((ram + stack + HEADROOM)) -gt $RAM_LEN ] && { stack=$free; note="all remaining RAM (headroom rule)"; }; fi
  link_try "$od" "$out" "$OUT/$img.map" $stack 0x0000FFF8 0x00001000 || { cat "$od/link.log"; exit 1; }
  printf "%-34s flash=%6d (%4.1f%%)  static_ram=%5d  stack=%5d  free=%4d  [%s]\n" "$img" "$flash" "$(echo "$flash*100/65536" | bc -l)" "$ram" "$stack" "$((RAM_LEN - ram - stack))" "$note" | tee -a "$OUT/sizes.txt"
  echo "$out" >> "$HERE/.images.new"
}

# ---- Embench-IoT: one image per benchmark ----
BMS="aha-mont64 crc32 cubic edn huffbench matmult-int minver nbody nettle-aes nettle-sha256 nsichneu picojpeg qrduino sglib-combined slre st statemate ud wikisort"
build_embench() {
  local n=$1 opt=$2 img="embench_${1//-/_}_$2"
  want "$img" || return 0
  local od; od=$(newdir "$img"); local E="$V/embench-iot" m=${n//-/_} objs=()
  common "$opt" "$od" 1
  cc "$opt" "$od" -I"$E/support" -- "$E/support/beebsc.c"
  echo "BM($m)" > "$od/list.h"
  for s in "$E"/src/$n/*.c; do
    cc "$opt" "$od" -DCPU_MHZ=1 -Dbenchmark=bm_${m}_run -Dinitialise_benchmark=bm_${m}_init \
       -Dverify_benchmark=bm_${m}_verify -Dwarm_caches=bm_${m}_warm -I"$E/src/$n" -I"$E/support" \
       -I"$E/config/arm/boards/generic" -I"$E/config/arm/chips/cortex-m4" -- "$s"
  done
  cc "$opt" "$od" -I"$od" -DIMAGE_NAME="\"$img\"" -- "$ORIG/embench/main.c"
  link "$img" "$od" rest
  rm -rf "$od"
}
build_crc() {
  local opt=$1 img=crc_$1; want "$img" || return 0
  local od; od=$(newdir "$img"); common "$opt" "$od" 8
  cc "$opt" "$od" -- "$HERE/crc/main.c"
  link "$img" "$od" rest; rm -rf "$od"
}
build_uecc() {
  local opt=$1 variant=$2 img="uecc_$2_$1"; want "$img" || return 0
  local od; od=$(newdir "$img"); common "$opt" "$od" 7
  local src="$V/micro-ecc"
  local f=(-DuECC_SUPPORTS_secp160r1=0 -DuECC_SUPPORTS_secp192r1=0 -DuECC_SUPPORTS_secp224r1=0 -DuECC_SUPPORTS_secp256k1=0 \
          -DuECC_SUPPORT_COMPRESSED_POINT=0 -I"$V/crypto-algorithms" -I"$ORIG/compat")
  if [ "$variant" = asm ]; then
    src="$od/mecc"; mkdir -p "$src"
    for x in "$V"/micro-ecc/*.inc "$V"/micro-ecc/*.h "$V"/micro-ecc/uECC.c; do
      sed 's/\.syntax divided/.syntax unified/' "$x" > "$src/$(basename "$x")"
    done
  else
    f+=(-DuECC_PLATFORM=uECC_arch_other -DuECC_WORD_SIZE=4)
  fi
  f+=(-I"$src")
  cc "$opt" "$od" "${f[@]}" -w -- "$src/uECC.c"
  cc "$opt" "$od" "${f[@]}" -- "$V/crypto-algorithms/sha256.c"
  cc "$opt" "$od" "${f[@]}" -- "$ORIG/uecc/main.c"
  # G3507 run measured at most 1704 bytes of stack for both variants; 2048 leaves 344 bytes of margin
  link "$img" "$od" 2048; rm -rf "$od"
}
build_hs() {
  local opt=$1 img=heatshrink_$1; want "$img" || return 0
  local od; od=$(newdir "$img"); common "$opt" "$od" 2
  local f=(-DHEATSHRINK_DYNAMIC_ALLOC=0 -I"$V/heatshrink")
  cc "$opt" "$od" "${f[@]}" -- "$V/heatshrink/heatshrink_encoder.c"
  cc "$opt" "$od" "${f[@]}" -- "$V/heatshrink/heatshrink_decoder.c"
  cc "$opt" "$od" "${f[@]}" -- "$HERE/text_hs/main.c"
  link "$img" "$od" rest; rm -rf "$od"
}
build_jsmn() {
  local opt=$1 img=jsmn_$1; want "$img" || return 0
  local od; od=$(newdir "$img"); common "$opt" "$od" 1
  cc "$opt" "$od" -I"$V/jsmn" -- "$HERE/text_jsmn/main.c"
  link "$img" "$od" rest; rm -rf "$od"
}
build_printf() {
  local opt=$1 variant=$2 img="printf_$2_$1"; want "$img" || return 0
  local od; od=$(newdir "$img"); common "$opt" "$od" 1
  local f=()
  case "$variant" in
    ti) f=(-DFMT_TI);;
    eyalroz) f=(-DFMT_EYALROZ -I"$V/printf/src");;
    eyalroz_nofloat) f=(-DFMT_EYALROZ -DNO_FLOAT -I"$V/printf/src" -DPRINTF_SUPPORT_DECIMAL_SPECIFIERS=0 -DPRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS=0 -DPRINTF_SUPPORT_LONG_LONG=0);;
    none) f=();;
  esac
  case "$variant" in eyalroz*) cc "$opt" "$od" "${f[@]}" -- "$V/printf/src/printf/printf.c";; esac
  cc "$opt" "$od" "${f[@]}" -- "$ORIG/printf_cmp/main.c"
  link "$img" "$od" rest; rm -rf "$od"
}

: > "$HERE/.images.new"
if [ -z "$FILTER" ]; then : > "$OUT/sizes.txt"; : > "$OUT/nofit.txt"; fi
echo "image                              flash(of 65536)       static RAM (of 4096)   stack  free"
build_crc O2
for opt in O2 Os; do for v in none ti eyalroz eyalroz_nofloat; do build_printf $opt $v; done; done
build_jsmn O2
build_uecc O2 asm
build_uecc O2 c
build_hs O2
for opt in O2 Os; do for n in $BMS; do build_embench $n $opt; done; done
if [ -z "$FILTER" ]; then mv "$HERE/.images.new" "$HERE/images.txt"; else cat "$HERE/.images.new"; rm "$HERE/.images.new"; fi
rm -rf "$B"
echo; echo "Sizes: $OUT/sizes.txt   Did not fit: $OUT/nofit.txt"
[ -z "$FILTER" ] && echo "Image list: $HERE/images.txt"
true
