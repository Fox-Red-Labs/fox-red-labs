#!/bin/bash
# Build pass of the sweep for the LP-MSPM33C321A: every SDK example with a ticlang makefile, built as shipped
# (section 6 of prebuild.sh, alone). Output: out/mspm33c321a/sweep/ and sweep/build.csv.
OUT="${OUT:-$PWD/out/mspm33c321a}"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; EX="$SDK/examples/nortos/LP_MSPM33C321A"
cd "$OUT" || exit 1
mkdir -p sweep; echo "kind,category,example,result,seconds" > sweep/build.csv
find "$SDK/examples" -path '*LP_MSPM33C321A*' -path '*/ticlang/makefile' | sort | while read -r mk; do
  free=$(df -m "$OUT" | awk 'NR==2{print $4}'); [ "$free" -lt 2000 ] && { echo "STOP: under 2 GB free" >> sweep/build.csv; exit 4; }
  t="$(dirname "$mk")"; ex="$(dirname "$t")"; case "$ex" in */freertos) ex="$(dirname "$ex")" ;; esac
  rel="${ex#$EX/}"; rel="${rel#$SDK/examples/rtos/LP_MSPM33C321A/}"; d="sweep/$(echo "$rel" | tr '/' '_')"
  kind=nortos; case "$mk" in */rtos/*) kind=rtos ;; esac
  rm -rf "$d"; cp -R "$ex" "$d"; tt="$d/${t#$ex/}"; s=$(date +%s)
  if make -C "$tt" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$d.log" 2>&1
  then r=built; else r=failed; fi
  echo "$kind,$(dirname "$rel"),$(basename "$rel"),$r,$(( $(date +%s) - s ))" >> sweep/build.csv
done
echo "sweep: $(grep -c ',built,' sweep/build.csv) built, $(grep -c ',failed,' sweep/build.csv) failed"
