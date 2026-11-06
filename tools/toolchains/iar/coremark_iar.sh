#!/bin/bash
# Runs INSIDE the frl-iar container (on the build host): EEMBC CoreMark with IAR C/C++ Compiler, bare metal.
#   coremark_iar.sh <l1306|g3519|m33> <high-speed|high-size|balanced> <mhz> [iterations]
# Same unmodified EEMBC files and the same core_portme as the TI Arm Clang and GCC builds, so the
# score sits beside theirs. Output: $FRL_TI_ROOT/frl-out/<board>-<opt>-<mhz>/coremark.out (+ .map, size.txt).
# Optimisation: high-speed is -Ohs, high-size is -Ohz, balanced is -Oh.
set -e
. /etc/profile.d/iar.sh; TOOLKIT_DIR=${TOOLKIT_DIR:-/opt/iar/cxarm/arm}
[ -d /devsup/cxarm-10.10.2/arm ] && cp -rsn /devsup/cxarm-10.10.2/arm/. /opt/iar/cxarm/arm/ 2>/dev/null
B=$1; OPT=$2; MHZ=$3; ITER=${4:-3000}
TI=${FRL_TI_ROOT:-/opt/ti}; SRC=$TI/frl-src; CM=$SRC/third-party/coremark
case $OPT in high-speed) O=-Ohs;; high-size) O=-Ohz;; balanced) O=-Oh;; *) echo "opt?"; exit 2;; esac
M0=$TI/mspm0_sdk_2_11_00_07; M33=$TI/mspm33_sdk_1_04_00_00
case $B in
  l1306) CPU=Cortex-M0+; SDK=$M0; P=$SRC/mspm0l1306/bench/coremark; G=$SRC/gen/l1306-$MHZ
         START=$M0/examples/nortos/LP_MSPM0L1306/driverlib/empty/iar/startup_mspm0l130x_iar.c
         ICF=$M0/examples/nortos/LP_MSPM0L1306/driverlib/empty/iar/mspm0l1306.icf
         LIB=$M0/source/ti/driverlib/lib/iar/m0p/mspm0l11xx_l13xx/driverlib.a; STACK=0xC00; DEV=__MSPM0L1306__; XTRA=() ;;
  g3519) CPU=Cortex-M0+; SDK=$M0; P=$SRC/mspm0l1306/bench/coremark; G=$SRC/gen/g3519-$MHZ
         D=$M0/examples/nortos/LP_MSPM0G3519/driverlib/empty/iar
         START=$M0/source/ti/devices/msp/m0p/startup_system_files/iar/startup_mspm0g351x_iar.c
         ICF=$(ls $D/*.icf | head -1); LIB=$M0/source/ti/driverlib/lib/iar/m0p/mspm0gx51x/driverlib.a; STACK=0x1000; DEV=__MSPM0G3519__; XTRA=() ;;
  m33)   CPU=Cortex-M33; SDK=$M33; P=$SRC/mspm33c321a/bench/coremark; G=$SRC/gen/m33-$MHZ
         D=$M33/examples/nortos/LP_MSPM33C321A/driverlib/empty/iar
         START=$(ls $D/startup*iar*.c | head -1); ICF=$(ls $D/*.icf | head -1)
         LIB=$(ls $M33/source/ti/driverlib/lib/iar/m33/*/driverlib.a | head -1); STACK=0x2000; DEV=__MSPM33C321A__; CM_LOG=${CM_LOG:-2048}; XTRA=(--fpu=VFPv5_sp "-DCPU_HZ=${MHZ}000000u" "-DBOARD_HEADER=\"ti_msp_dl_config.h\"") ;;
  *) echo "board?"; exit 2 ;;
esac
# Knobs for like-for-like work (all optional): TAG=<suffix for the out folder>, XCC="<extra iccarm flags>",
# LISTS=1 writes assembly listings (lists/*.lst), MFC=1 adds --mfc (multi-file compilation), NOSC=1 adds --no_size_constraints.
OUT=$TI/frl-out/$B-$OPT-$MHZ${TAG:+-$TAG}; rm -rf "$OUT"; mkdir -p "$OUT"; cd "$OUT"
CFLAGS=(--cpu=$CPU -D$DEV -D__USE_SYSCONFIG__ "${XTRA[@]}" -e $O --dlib_config "$TOOLKIT_DIR/inc/c/DLib_Config_Normal.h"
        -I"$CM" -I"$P" -I"$G" -I"$SDK/source" -I"$SDK/source/third_party/CMSIS/Core/Include"
        -DITERATIONS=$ITER -DPERFORMANCE_RUN=1 -DCM_LOG_SIZE=${CM_LOG:-800} -DHAS_FLOAT=0
        '-DCOMPILER_VERSION="IAR CXARM 10.10.2"' "-DCOMPILER_FLAGS=\"$O bare metal, $MHZ MHz\"")
[ "${NOSC:-0}" = 1 ] && CFLAGS+=(--no_size_constraints)
[ -n "${XCC:-}" ] && CFLAGS+=($XCC)
LST() { [ "${LISTS:-0}" = 1 ] && { mkdir -p lists; echo "-lA lists/$1.lst"; }; }
if [ "${MFC:-0}" = 1 ]; then   # multi-file compilation (--mfc): the six CoreMark files as one unit; main.c stays apart: it defines main
  mkdir -p mfc; srcs=(); for f in core_list_join core_matrix core_state core_util core_portme core_main; do
    [ -f "$P/$f.c" ] && srcs+=("$P/$f.c") || srcs+=("$CM/$f.c"); done
  iccarm --mfc "${CFLAGS[@]}" -Dmain=coremark_main "${srcs[@]}" -o mfc/
  cp mfc/*.o . 2>/dev/null || true
  [ -f "$P/main.c" ] && src="$P/main.c" || src="$CM/main.c"; iccarm "${CFLAGS[@]}" "$src" -o main.o
else
for f in core_list_join core_matrix core_state core_util core_portme main; do
  [ -f "$P/$f.c" ] && src="$P/$f.c" || src="$CM/$f.c"; iccarm "${CFLAGS[@]}" $(LST $f) "$src" -o $f.o; done
iccarm "${CFLAGS[@]}" $(LST core_main) -Dmain=coremark_main "$CM/core_main.c" -o core_main.o
fi
iccarm "${CFLAGS[@]}" $(LST ti_msp_dl_config) "$G/ti_msp_dl_config.c" -o ti_msp_dl_config.o
iccarm "${CFLAGS[@]}" "$START" -o startup.o
sed -E "s/(define symbol __ICFEDIT_size_cstack__ *= *)0x[0-9A-Fa-f]+/\1$STACK/" "$ICF" > run.icf
grep -n "size_cstack__ *=" run.icf
ilinkarm --config run.icf --map coremark.map --semihosting=none \
  --entry __iar_program_start ./*.o "$LIB" -o coremark.out 2>&1 | tee link.txt
grep -E "bytes of (readonly|readwrite)" link.txt | tee size.txt
# DSLite refuses an ELF whose first load block is not a multiple of 16 bytes (the vector table is 276 bytes), so
# also write an Intel HEX image padded with 0xFF over the whole flash footprint, rounded up to 256 bytes.
END=$(python3 - <<'PY'
import struct
d = open("coremark.out", "rb").read()
phoff, = struct.unpack_from("<I", d, 0x1C); phentsize, phnum = struct.unpack_from("<HH", d, 0x2A)
end = 0
for i in range(phnum):
    t, off, va, pa, fsz, msz = struct.unpack_from("<6I", d, phoff + i * phentsize)
    if t == 1 and pa >= 0x10000000 and pa < 0x10100000 and fsz:
        end = max(end, pa + fsz)
print("0x%X" % (((end + 255) // 256) * 256 - 1))
PY
)
ielftool --fill "0xFF;0x10000000-$END" --ihex coremark.out coremark.hex | tail -1
