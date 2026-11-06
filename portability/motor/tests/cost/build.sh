#!/bin/bash
# Cost matrix, flash and RAM: build isr_body.c (and guard.c when the guard is on) for every variant, core and toolchain
# and write sizes.csv. Usage: build.sh [out dir]   (default $TMPDIR/frl-cost)
# Toolchains: arm-none-eabi-gcc (STM32CubeCLT 14.3), TI Arm Clang 5.1.1.LTS (tiarmclang), TI C2000 CGT 25.11.1.LTS (cl2000).
# Sizes: text includes read-only data; objects only, not linked (see EXPECTATIONS.md).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; CORE="$HERE/../../core"
OUT="${1:-${TMPDIR:-/tmp}/frl-cost}"; mkdir -p "$OUT"
GCC=arm-none-eabi-gcc; GSIZE=arm-none-eabi-size
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
C28=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
# the last commit that touched the code under test; "-dirty" when the working tree differs from it
COMMIT=$(git -C "$HERE" log -1 --format=%h -- "$CORE" "$HERE/isr_body.c")
git -C "$HERE" diff --quiet HEAD -- "$CORE" "$HERE/isr_body.c" 2>/dev/null || COMMIT="$COMMIT-dirty"
CSV="$OUT/sizes.csv"; echo "toolchain,core,num,trig,variant,text,data,bss,commit,flags" > "$CSV"
varflags() { case $1 in   # bash 3.2 on macOS has no associative arrays
  A) echo "-DFRL_TIER=0 -DFRL_F_GUARD=0";;
  B) echo "-DFRL_TIER=0 -DFRL_F_GUARD=1";;
  C) echo "-DFRL_TIER=1 -DFRL_F_GUARD=0";;
  D) echo "-DFRL_TIER=2 -DFRL_F_GUARD=0 -DFRL_F_HALL_TRACK=0 -DFRL_ANGLE_FROM_HALL_SECTOR";;
  E) echo "-DFRL_TIER=2 -DFRL_F_GUARD=0";;
  F) echo "-DFRL_TIER=2 -DFRL_F_GUARD=1";; esac; }

arm_sizes() { "$@" | awk 'NR>1{t+=$1;d+=$2;b+=$3} END{printf "%d,%d,%d", t, d, b}'; }

build() {   # toolchain core numname trigname variant numv trigv
	local tc=$1 core=$2 numname=$3 trig=$4 v=$5 numv=$6 trigv=$7
	local d="$OUT/$tc-$core-$numname-$trig-$v"; rm -rf "$d"; mkdir -p "$d"
	local vf; vf=$(varflags $v); local defs="-DFRL_NUM=$numv -DFRL_TRIG=$trigv $vf" guard=0 CC cf sz objs
	[[ "$vf" == *"FRL_F_GUARD=1"* ]] && guard=1
	if [ "$tc" = c2000 ]; then
		cf="--c99 --opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --diag_suppress=10063 $defs -I$CORE -I$HERE -I$C28/include"
		"$C28/bin/cl2000" $cf -c "$HERE/isr_body.c" --output_file="$d/isr_body.obj" >"$d/log" 2>&1 || { echo "FAIL $d"; tail -3 "$d/log"; return; }
		objs=("$d/isr_body.obj")
		if [ $guard = 1 ]; then "$C28/bin/cl2000" $cf -c "$CORE/guard.c" --output_file="$d/guard.obj" >>"$d/log" 2>&1 || { echo "FAIL guard $d"; return; }; objs+=("$d/guard.obj"); fi
		sz=$(python3 -I "$HERE/c28_sizes.py" "$C28/bin/ofd2000" "${objs[@]}")
		echo "$tc,$core,$numname,$trig,$v,$sz,$COMMIT,\"$cf\"" >> "$CSV"; return
	fi
	case "$tc:$core" in
	gcc:m0p)   CC="$GCC -mcpu=cortex-m0plus -mthumb" ;;
	gcc:m33)   CC="$GCC -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16" ;;
	clang:m0p) CC="$CGT/bin/tiarmclang -mcpu=cortex-m0plus -mfloat-abi=soft" ;;
	clang:m33) CC="$CGT/bin/tiarmclang -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16" ;;
	esac
	cf="-std=gnu99 -O2 -fno-common -ffunction-sections -fdata-sections $defs -I$CORE -I$HERE"
	$CC $cf -c "$HERE/isr_body.c" -o "$d/isr_body.o" >"$d/log" 2>&1 || { echo "FAIL $d"; tail -3 "$d/log"; return; }
	objs=("$d/isr_body.o")
	if [ $guard = 1 ]; then $CC $cf -c "$CORE/guard.c" -o "$d/guard.o" >>"$d/log" 2>&1 || { echo "FAIL guard $d"; return; }; objs+=("$d/guard.o"); fi
	if [ "$tc" = gcc ]; then sz=$(arm_sizes "$GSIZE" "${objs[@]}"); else sz=$(arm_sizes "$CGT/bin/tiarmsize" --format=berkeley "${objs[@]}"); fi
	echo "$tc,$core,$numname,$trig,$v,$sz,$COMMIT,\"$CC $cf\"" >> "$CSV"
}

for v in A B C D E F; do
	for tc in gcc clang; do
		build $tc m0p iq24  table $v 2 1
		build $tc m0p q15   table $v 1 1
		build $tc m33 float table $v 0 1
		build $tc m33 float libm  $v 0 0
		build $tc m33 iq24  table $v 2 1
	done
	build c2000 f28 float table $v 0 1
	build c2000 f28 iq24  table $v 2 1
done
echo "wrote $CSV ($(($(wc -l < "$CSV") - 1)) builds)"
