#!/bin/bash
# MacBook side. Gathers what the IAR CoreMark build needs (small files only) and pushes it to the build host.
#   coremark_prep.sh l1306|g3519|m33
# Pushes to $IAR_HOST:$IAR_SRC (the IAR build machine; IAR_SRC defaults to $FRL_TI_ROOT/frl-src/): third-party/coremark, the board's CoreMark port and common timer,
# and SysConfig output made with --compiler iar from the same .syscfg the TI Arm Clang builds use
# (L1306 32 MHz: mspm0l1306/bench/coremark/syscfg32; G3519 32 and 80 MHz and M33 32 and 160 MHz: the SDK
# examples named in each board's prebuild.sh). Nothing is compiled here.
set -e
B=$1; HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
S128=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
M0=/Applications/ti/mspm0_sdk_2_11_00_07; M33=/Applications/ti/mspm33_sdk_1_04_00_00
T=$(mktemp -d); mkdir -p "$T/third-party" ; cp -R "$REPO/third-party/coremark" "$T/third-party/"
gen() { mkdir -p "$T/gen/$3"; "$S128" --compiler iar --product "$1/.metadata/product.json" --output "$T/gen/$3" "$2" >/dev/null; }
case $B in
  l1306) mkdir -p "$T/mspm0l1306/bench"; cp -R "$REPO/mspm0l1306/bench/coremark" "$REPO/mspm0l1306/bench/common" "$T/mspm0l1306/bench/"
         rm -rf "$T"/mspm0l1306/bench/coremark/build-*
         gen $M0 "$REPO/mspm0l1306/bench/coremark/syscfg32/gpio_toggle_output.syscfg" l1306-32 ;;
  g3519) mkdir -p "$T/mspm0l1306/bench"; cp -R "$REPO/mspm0l1306/bench/coremark" "$REPO/mspm0l1306/bench/common" "$T/mspm0l1306/bench/"
         rm -rf "$T"/mspm0l1306/bench/coremark/build-* "$T"/mspm0l1306/bench/coremark/syscfg*
         EX=$M0/examples/nortos/LP_MSPM0G3519/driverlib
         gen $M0 $EX/gpio_toggle_output/gpio_toggle_output.syscfg g3519-32
         gen $M0 $EX/sysctl_mclk_syspll/sysctl_mclk_syspll.syscfg g3519-80 ;;
  m33)   mkdir -p "$T/mspm33c321a/bench"; cp -R "$REPO/mspm33c321a/bench/coremark" "$REPO/mspm33c321a/bench/m33_timer.h" "$T/mspm33c321a/bench/"
         EX=$M33/examples/nortos/LP_MSPM33C321A
         gen $M33 $EX/driverlib/empty/empty.syscfg m33-32
         gen $M33 $EX/driverlib/sysctl_mclk_syspll/sysctl_mclk_syspll.syscfg m33-160 ;;
  *) echo "board: l1306 g3519 m33"; exit 2 ;;
esac
du -sk "$T" | awk '{print $1 " KB staged"}'
rsync -a "$T/" "${IAR_HOST:?set IAR_HOST to the IAR build machine (ssh host name)}:${IAR_SRC:-${FRL_TI_ROOT:-/opt/ti}/frl-src/}" && echo pushed; rm -rf "$T"
