#!/bin/bash
# Cross-build between the MCU+ SDK for the AM263Px (LP-AM263P, R5F) and the MMWAVE-L-SDK for the IWRL6432 (M4F), both ways,
# for the driver examples both ship under the same name (examples/drivers/<category>/<example>). Both SDKs share the MCU+
# layout: the app .c and .h files sit at the example root, main.c and the SysConfig file sit in the board/core folder.
# Baseline: the target's own example untouched. Swapped: the source SDK's example-root .c and .h files dropped in, nothing else
# (the target keeps its main.c, SysConfig file and linker). Build only. Rung 2 of the ladder.
#   crossbuild_am263p_mmwave.sh <work dir> <results jsonl> <am263p_to_mmwave | mmwave_to_am263p>
W="$1"; OUT="$2"; DIR=${3:-am263p_to_mmwave}; mkdir -p "$W/logs"
AM=$HOME/ti/mcu_plus_sdk_am263px_26_01_00_08; MM=$HOME/ti/mmwave_l_sdk_05_05_04_02
CGT=/Applications/ti/ti-cgt-armllvm_release
# the SDK's imports.mak sets MKDIR and friends for Windows (cygwin) and Linux only; name them for macOS
MK=(PROFILE=release TOOLS_PATH=$HOME/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=$CGT SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0 "MKDIR=mkdir -p" "RMDIR=rm -rf" "RM=rm -f" COPY=cp CAT=cat TOUCH=touch)
if [ "$DIR" = am263p_to_mmwave ]; then A=$AM; B=$MM; BD=xwrL64xx-evm; CORE=m4fss0-0_freertos; FROM=LP_AM263P; TO=IWRL6432BOOST
else A=$MM; B=$AM; BD=am263px-lp; CORE=r5fss0-0_nortos; FROM=IWRL6432BOOST; TO=LP_AM263P; fi
for cat in $(comm -12 <(ls $A/examples/drivers) <(ls $B/examples/drivers)); do
for e in $(comm -12 <(ls $A/examples/drivers/$cat) <(ls $B/examples/drivers/$cat)); do
  [ -d "$B/examples/drivers/$cat/$e/$BD/$CORE/ti-arm-clang" ] || continue
  [ -n "${ONLY:-}" ] && ! [[ "$cat/$e" =~ $ONLY ]] && continue          # ONLY=<regex> limits the run
  [ -n "${COMPAT:-}" ] && [ "${MODES:-}" = swapped ] && true
  for mode in ${MODES:-baseline swapped}; do
    d="$W/$DIR/$mode/$cat/$e"; rm -rf "$d"; mkdir -p "$(dirname "$d")"; cp -R "$B/examples/drivers/$cat/$e" "$d"
    if [ $mode = swapped ]; then for c in "$A/examples/drivers/$cat/$e"/*.c "$A/examples/drivers/$cat/$e"/*.h; do [ -f "$c" ] && cp "$c" "$d/"; done; fi
    # COMPAT=<header>: force-include a mapping header into every file the project compiles (rung 3: glue, app untouched)
    if [ $mode = swapped ] && [ -n "${COMPAT:-}" ]; then
      python3 -c "import sys;p=sys.argv[1];s=open(p).read();open(p,'w').write(s.replace('CFLAGS_common := \\\\\n','CFLAGS_common := \\\\\n\\t-include '+sys.argv[2]+' \\\\\n',1))" "$d/$BD/$CORE/ti-arm-clang/makefile" "$COMPAT"; fi
    T="$d/$BD/$CORE/ti-arm-clang"; N=$(grep -m1 '^OUTNAME' $T/makefile | sed 's/.*:=//;s/\$(PROFILE)/release/;s/\$(CONFIG)/release/' | tr -d ' ')
    log="$W/logs/$DIR-$mode-$cat-$e.log"
    rm -f "$T"/*.out "$T"/*.appimage "$T"/*.map                      # both SDKs ship a prebuilt .out beside the makefile
    ( cd "$T" && make -j2 $N MCU_PLUS_SDK_PATH=$B "${MK[@]}" ) >"$log" 2>&1
    o=$(ls "$T"/*.out 2>/dev/null | head -1); ok=$([ -n "$o" ] && echo true || echo false)
    size=""; [ -n "$o" ] && size=$("$CGT/bin/tiarmsize" "$o" | tail -1 | awk '{print $1}')
    err=$(grep -E "error:|undefined symbol|No rule to make" "$log" | sed -E 's/^.*error: //' | sort -u | head -4 | tr '\n' ';' | cut -c1-400 | tr '"\\' "''")
    same=""; [ $mode = swapped ] && same=$(diff -q <(cat "$A/examples/drivers/$cat/$e"/*.c) <(cat "$B/examples/drivers/$cat/$e"/*.c) >/dev/null 2>&1 && echo identical || echo differs)
    echo "{\"src\":\"$FROM:drivers/$cat/$e\",\"dst\":\"$TO:drivers/$cat/$e\",\"mode\":\"$mode${COMPAT:++compat}\",\"ok\":$ok,\"text\":${size:-null},\"app_source\":\"$same\",\"first_error\":\"$err\",\"date\":\"$(date +%F)\"}" >>"$OUT"
    echo "$DIR $mode $cat/$e ok=$ok ${size:-} $same ${err:0:80}"
    [ -n "${KEEP:-}" ] || rm -rf "$T/obj"
  done
done; done
