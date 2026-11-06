#!/bin/bash
# Build the Mac host run and the three board images, run the emulator checks, write results/build-log.txt.
# Needs: arm-none-eabi-gcc on PATH (or GCC_BIN), MSPM0 SDK, MSPM33 SDK, python3 with unicorn (EMU_PY, default python3), clang.
set -e
cd "$(dirname "$0")"; mkdir -p results
[ -n "$GCC_BIN" ] && export PATH="$GCC_BIN:$PATH"
EMU_PY=${EMU_PY:-python3}
{ echo "# wasm3 port build sizes $(date +%Y-%m-%d)"; arm-none-eabi-gcc --version | head -1; echo "wasm3 v0.9.0 (0cd38327), app.wasm $(wc -c < app/app.wasm) bytes"; } > results/build-log.txt
host/build.sh > results/host-run.txt 2>&1 || true
OUT=$PWD/build-g3519 boards/g3519/build.sh 16384 2048 2>&1 | grep -E "^g3519" >> results/build-log.txt
NSTACK=5120 BOARD=g3507 OUT=$PWD/build-g3507-tiny boards/g3519/build.sh 6144 1024 2>&1 | grep -E "^g3507" | sed 's/^g3507/g3507-tiny/' >> results/build-log.txt
BOARD=g3507 OUT=$PWD/build-g3507-std boards/g3519/build.sh 16384 2048 2>&1 | grep -E "^g3507" | sed 's/^g3507/g3507-std/' >> results/build-log.txt || true
OUT=$PWD/build-m33 boards/m33c321a/build.sh 16384 4096 2>&1 | grep -E "^m33" >> results/build-log.txt
OUT=$PWD/build-m33-emu FIB_N=20 BENCH_N=20000 boards/m33c321a/build.sh 16384 4096 > /dev/null 2>&1
$EMU_PY tests/emu_check.py build-g3519 g3519 > results/emu-g3519.txt 2>&1 || true
$EMU_PY tests/emu_check.py build-g3507-tiny g3507 > results/emu-g3507-tiny.txt 2>&1 || true
$EMU_PY tests/emu_check.py build-m33-emu m33 > results/emu-m33.txt 2>&1 || true
rm -f build-*/*.o build-m33-emu/*.out build-m33-emu/*.elf 2>/dev/null || true
tail -3 results/emu-*.txt | grep -E "==>|PASS|FAIL"
cat results/build-log.txt
