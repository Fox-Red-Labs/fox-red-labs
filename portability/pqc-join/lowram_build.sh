#!/bin/bash
# Build the shared post-quantum core with mldsa-native's -DMLD_CONFIG_REDUCE_RAM, through the existing board build scripts.
# BUILD ONLY: never touches a probe or a board. The board scripts are patched in a temp copy (one added -D on the ML-DSA flags, the
# SysConfig path, the tools folder); the originals are not edited.
#   lowram_build.sh g3519|g5187|m33 [-O3|-Os]  [NOSIGN=1 (g5187: keygen and verify only, no signing)] [STACK=<bytes>] [OUT=<dir>] [LOWRAM_FLAG= for the default build, to compare]
# SysConfig output is regenerated here from the SDK's own example (g3519, m33); g5187 uses the committed mspm0g5187/bench/gp/syscfg.
# SYSCONFIG_DIR must hold the pinned SysConfig (default: first of /Applications/ti/sysconfig_1.26.3, /Volumes/Seagate/ti/sysconfig_1.26.3).
# Download: ti.com/tool/download/SYSCONFIG/1.26.3.4558 (macOS .dmg, no login on 2026-11-02); install with
#   installbuilder.sh --mode unattended --prefix <dir>   from inside the mounted .dmg's setup app (Contents/MacOS).
set -eu
SYSCONFIG_PIN=1.26.3
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
BD="${1:?usage: lowram_build.sh g3519|g5187|m33 [-O3|-Os]}"; OPT="${2:--O3}"
if [ -z "${SYSCONFIG_DIR:-}" ]; then
  for d in /Applications/ti/sysconfig_$SYSCONFIG_PIN /Volumes/Seagate/ti/sysconfig_$SYSCONFIG_PIN; do [ -x "$d/sysconfig_cli.sh" ] && SYSCONFIG_DIR=$d && break; done
fi
SYSCFG="${SYSCONFIG_DIR:-}/sysconfig_cli.sh"
[ -x "$SYSCFG" ] || { echo "SysConfig $SYSCONFIG_PIN not found: set SYSCONFIG_DIR=<folder holding sysconfig_cli.sh> (ti.com/tool/download/SYSCONFIG/1.26.3.4558)." >&2; exit 2; }
V="$("$SYSCFG" --version 2>/dev/null | head -1)"
case "$V" in $SYSCONFIG_PIN+*) ;; *) echo "SysConfig at $SYSCONFIG_DIR is '$V', this script is pinned to $SYSCONFIG_PIN." >&2; exit 2;; esac
T="${TMPDIR:-/tmp}/lowram-$BD-$$"; mkdir -p "$T"
patch() { sed -e "s#\$(cd \"\$(dirname \"\$0\")\" \&\& pwd)#$2#" -e "s#SYSCFG=/Applications/ti/sysconfig_[0-9.]*/sysconfig_cli.sh#SYSCFG=$SYSCFG#" -e "s#\$(dirname \"\$0\")/../../tools/frl#$REPO/tools/frl#" -e 's/-DMLD_CONFIG_NO_RANDOMIZED_API -I\$MD/-DMLD_CONFIG_NO_RANDOMIZED_API '"${LOWRAM_FLAG--DMLD_CONFIG_REDUCE_RAM}"' -I$MD/' "$1" > "$3"; chmod +x "$3"; }
case "$BD" in
 g3519) patch "$REPO/portability/boards/mspm0g3519/build_pqc.sh" "$REPO/portability/boards/mspm0g3519" "$T/b.sh"
   OUT="${OUT:-$T/out}" GEN="$T/gen/g80" "$T/b.sh" "$OPT";;
 g5187) patch "$REPO/portability/boards/mspm0g3519/build_pqc.sh" "$REPO/portability/boards/mspm0g3519" "$T/b.sh"
   BOARD=LP_MSPM0G5187 STARTUP=startup_mspm0g518x_ticlang.c LIBDIR=mspm0gx51x GEN="$REPO/mspm0g5187/bench/gp/syscfg" NO_BANK=1 \
   OUT="${OUT:-$T/out}" EXTRA_CC=-fdata-sections EXTRA_LD="-Wl,--unused_section_elimination=on" PQLOG=3072 PQC_DEFS="${NOSIGN:+-DPQC_DSA_NOSIGN}" STACK="${STACK:-$([ "$OPT" = -Os ] && echo 13312 || echo 19456)}" "$T/b.sh" "$OPT";;
 m33) patch "$REPO/mspm33c321a/tools/build_pqc.sh" "$REPO/mspm33c321a/tools" "$T/b.sh"
   sed -i.bak 's#\$(cd "\$(dirname "\$0")/.." \&\& pwd)#'"$REPO/mspm33c321a"'#' "$T/b.sh"
   TOOLPATHS_FAMILY=mspm33-cc; . "$REPO/tools/frl/toolpaths.sh"   # SDK = MSPM33 SDK
   G="$T/run/gen${MHZ:-160}"; mkdir -p "$G"; EX="$SDK/examples/nortos/LP_MSPM33C321A/driverlib/sysctl_mclk_syspll"
   "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$EX/sysctl_mclk_syspll.syscfg" > "$G/sysconfig.log" 2>&1 || { echo "SysConfig failed:"; tail -5 "$G/sysconfig.log"; exit 1; }
   cp "$EX/ticlang/device_linker.cmd" "$G/device_linker.cmd"
   RUN="$T/run" "$T/b.sh" "${OPT#-}" "${MHZ:-160}";;
 *) echo "unknown board $BD" >&2; exit 2;;
esac
