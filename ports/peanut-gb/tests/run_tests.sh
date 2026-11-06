#!/bin/bash
# One command: bash tests/run_tests.sh
# Builds tests/test_bad_roms.c with -fsanitize=address,undefined against the PATCHED Peanut-GB copy and expects it to pass,
# then against the UNPATCHED copy and expects it to fail (so the test proves the patch is needed). Exit 0 only if both hold.
set -e
cd "$(dirname "$0")/.."
A=../arduboy
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1"
build() { # dir
  bash patches/apply.sh $1
  cc -std=gnu11 $SAN -w -I$A/include -Isrc -I$1 -c src/pgb_frontend.c -o $1/fe.o
  c++ -std=gnu++17 $SAN -w -I$A/include -c $A/src/frl_core.cpp -o $1/core.o
  cc -std=gnu11 $SAN -w -I$A/include -Isrc -I$1 -c tests/test_bad_roms.c -o $1/test.o
  c++ $SAN $1/fe.o $1/core.o $1/test.o -o $1/test
}
build out/test-patched
out/test-patched/test
PGB_UNPATCHED=1 build out/test-unpatched
for g in header banks; do
  if out/test-unpatched/test $g > out/test-unpatched/log-$g.txt 2>&1; then
    echo "FAIL: the unpatched header passed the $g tests, so they do not prove the patch"; exit 1
  fi
  echo "ok: unpatched header fails the $g tests under the sanitizers as expected:"
  grep -m1 "runtime error\|ERROR: AddressSanitizer" out/test-unpatched/log-$g.txt
done
