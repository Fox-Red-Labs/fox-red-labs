#!/bin/bash
# Build every SDK example that has an am263px-lp project, one at a time with the SDK makefile and TI Arm Clang 5.1.1, record the result,
# keep the .out in $OUT/sweep/ and clear the build products so the disk does not fill. Stops if free space falls under 2 GB.
#   sweep_build.sh [name-filter-regex]          writes $OUT/sweep/results.tsv: rel-path, status, build-seconds, text, data, bss, out-bytes
OUT="${OUT:-$PWD/out/am263p}"; E=$HOME/ti/mcu_plus_sdk_am263px_26_01_00_08/examples
HERE="$(cd "$(dirname "$0")" && pwd)"; mkdir -p "$OUT/sweep"; T="$OUT/sweep/results.tsv"; touch "$T"
SZ=/Applications/ti/ti-cgt-armllvm_release/bin/tiarmsize
cd "$E"
for d in $(find . -type d -path "*am263px-lp/*/ti-arm-clang" | sed 's|^\./||;s|/ti-arm-clang$||' | sort); do
  [[ -n "${1:-}" && ! "$d" =~ $1 ]] && continue
  grep -q "^$d	" "$T" && continue
  free=$(df -k /System/Volumes/Data | tail -1 | awk '{print int($4/1024)}'); [ "$free" -lt 2048 ] && { echo "STOP: free ${free} MB"; exit 2; }
  r=$("$HERE/build_sdk_example.sh" "$E/$d" | tail -1); st=$(echo "$r" | awk '{print $2}'); sec=$(echo "$r" | awk '{print $3}'); out=$(echo "$r" | awk '{print $4}')
  if [ "$st" = ok ]; then
    sz=$($SZ "$out" 2>/dev/null | tail -1 | awk '{print $1"\t"$2"\t"$3}'); b=$(stat -f %z "$out"); n=$(echo "$d" | tr '/' '_')
    cp "$out" "$OUT/sweep/$n.out"; echo -e "$d\tok\t$sec\t$sz\t$b" >> "$T"
  else
    n=$(echo "$d" | tr '/' '_'); cp "$E/$d/ti-arm-clang/build.log" "$OUT/sweep/$n.faillog" 2>/dev/null
    echo -e "$d\tfail\t$sec\t\t\t\t" >> "$T"
  fi
  (cd "$E/$d/ti-arm-clang" && rm -rf obj generated *.map *.lnkxml build.log *.mcelf* 2>/dev/null)
done
echo SWEEP DONE
