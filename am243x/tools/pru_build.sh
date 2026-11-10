#!/bin/bash
# Build the PRU toggle test (am243x/bench/pru) inside a copy of the SDK's pru_io/empty example (the SDK makefiles use paths relative to the SDK).
# Needs the run folder's env.sh sourced and the PRU compiler in ~/ti/ti-cgt-pru_2.3.3. Output: $O/frl_pru_toggle.out for ram_capture.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; X=$SDK/examples/pru_io/frl_pru_toggle; O=${O:-$HOME/tmp/_agent/runs/arrivals/am243x/pru}; mkdir -p $O
if [ ! -d $X ]; then cp -R $SDK/examples/pru_io/empty $X; rm -rf $X/am243x-evm $X/firmware/am243x-evm; fi
grep -rl "pru_io/empty" $X --include=makefile | xargs sed -i.b 's#pru_io/empty#pru_io/frl_pru_toggle#g'; find $X -name "*.b" -delete
cp $HERE/bench/pru/toggle.asm $X/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/main.asm
cp $HERE/bench/pru/host.c $X/empty_example.c
export CGT_TI_PRU_PATH=$HOME/ti/ti-cgt-pru_2.3.3
( cd $X/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt && make clean $MAKE_ARGS >/dev/null 2>&1; make all $MAKE_ARGS CGT_TI_PRU_PATH=$CGT_TI_PRU_PATH ) > $O/pru_fw.log 2>&1 || { echo "PRU firmware build failed: $O/pru_fw.log"; exit 1; }
"$HERE/tools/build_sdk_example.sh" $X/am243x-lp/r5fss0-0_freertos | tee $O/host_build.txt
cp $X/am243x-lp/r5fss0-0_freertos/ti-arm-clang/empty_pru_io.release.out $O/frl_pru_toggle.out
