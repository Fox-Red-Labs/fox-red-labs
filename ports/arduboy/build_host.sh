#!/bin/bash
# One command: bash build_host.sh <game> [out-dir]     (game = crate-confusion | snake | tileworld)
# Builds the game unmodified from third-party/arduboy-games/<game> against the Arduboy2 layer and the
# headless host backend. Output: <out-dir>/ab_<game> (default: out/host).
set -e
cd "$(dirname "$0")"
G=$1; [ -n "$G" ] || { echo "usage: build_host.sh <game> [out-dir]"; exit 1; }
O=${2:-out/host}; mkdir -p "$O"
GD=../../third-party/arduboy-games/$G
[ -d "$GD" ] || { echo "no such game: $G"; exit 1; }
CXX=${CXX:-c++}
# crate-confusion defines its own mode_t type, which clashes with the system one; rename it on the command line.
EXTRA=""
[ "$G" = crate-confusion ] && EXTRA="-DFRL_RENAME_MODE_T"
SRCS=$(ls $GD/*.cpp 2>/dev/null || true)
INO=$(ls $GD/*.ino)
OBJS=""
for f in $INO; do
  OBJ="$O/$G-$(basename $f).o"
  python3 tools/ino_protos.py $f $O/$G-$(basename $f).cpp
  $CXX -x c++ -std=gnu++17 -O1 -w -fno-exceptions -fno-rtti -Iinclude -I$GD -include tools/ino_prelude.h $EXTRA -c $O/$G-$(basename $f).cpp -o $OBJ
  OBJS="$OBJS $OBJ"
done
for f in $SRCS; do
  OBJ="$O/$G-$(basename $f).o"
  $CXX -std=gnu++17 -O1 -w -fno-exceptions -fno-rtti -Iinclude -I$GD -include tools/ino_prelude.h $EXTRA -c $f -o $OBJ
  OBJS="$OBJS $OBJ"
done
$CXX -std=gnu++17 -O1 -w -fno-exceptions -fno-rtti -Iinclude -c src/arduboy2.cpp -o $O/arduboy2.o
$CXX -std=gnu++17 -O1 -w -fno-exceptions -fno-rtti -Iinclude -c src/frl_core.cpp -o $O/frl_core.o
$CXX -std=gnu++17 -O1 -w -Iinclude -c host/ab_host.cpp -o $O/ab_host.o
$CXX $OBJS $O/arduboy2.o $O/frl_core.o $O/ab_host.o -lz -o $O/ab_$G
echo "built $O/ab_$G"
