#!/bin/bash
# LP-MSPM0G5187 values for the M0+ pack in ../mspm0g3519 (build_pqc.sh, build_sec.sh): 32 kB of RAM in ONE bank, so the stack stays in
# the single SRAM, the ML-DSA files are not built (its stack needs 31 to 52 kB), the unused join buffers are dropped at link
# and the RAM log is 3 kB.   ./build.sh pqc [-Os|-O3] | sec [-O2] [ti|uecc-asm|uecc-c]
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; G="$HERE/../mspm0g3519"
export BOARD=LP_MSPM0G5187 STARTUP=startup_mspm0g518x_ticlang.c LIBDIR=mspm0gx51x GEN="$HERE/../../../mspm0g5187/bench/gp/syscfg"
export OUT="${OUT:-$PWD/out/speed-run/lp-mspm0g5187}" NO_BANK=1
case "$1" in
  pqc) shift; NO_DSA=1 EXTRA_CC=-fdata-sections EXTRA_LD="-Wl,--unused_section_elimination=on" PQLOG=3072 STACK="${STACK:-19456}" "$G/build_pqc.sh" "$@" ;;
  sec) shift; STACK="${STACK:-8192}" "$G/build_sec.sh" "$@" ;;
esac
