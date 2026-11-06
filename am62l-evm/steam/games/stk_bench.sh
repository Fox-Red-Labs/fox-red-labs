#!/bin/sh
# Inside frl-steamcmd (Debian trixie arm64 with Xvfb, Mesa llvmpipe, Box64): SuperTuxKart 1.5 profile run,
# the arm64 build natively or the x86_64 build under Box64. AI drives; STK prints its frame statistics at the end.
# Usage: sh /g/src/stk_bench.sh arm64|x86_64 WIDTHxHEIGHT [seconds=60] [extra stk args]
set -u
ARCH=$1; RES=$2; SEC=${3:-60}; shift 3 2>/dev/null || shift $#
D=/g/stk/$( [ $ARCH = arm64 ] && echo a || echo x )/SuperTuxKart-1.5-linux-$ARCH
L=/g/logs/stk-$ARCH-$RES-$(date +%H%M%S).log
export HOME=/g/stk/home-$ARCH DISPLAY=:2; mkdir -p $HOME
pgrep -f "Xvfb :2" >/dev/null || { Xvfb :2 -screen 0 1920x1080x24 >/dev/null 2>&1 & sleep 2; }
RUN="$D/run_game.sh"; [ $ARCH = x86_64 ] && RUN="box64 $D/bin/supertuxkart"
export SUPERTUXKART_DATADIR=$D SUPERTUXKART_ASSETS_DIR=$D/data/ LD_LIBRARY_PATH=$D/lib
cd $D; s=$(date +%s)
$RUN --screensize=$RES --windowed --profile-time=$SEC --track=lighthouse --numkarts=4 --no-start-screen "$@" > $L 2>&1
echo "$ARCH $RES rc=$? wall=$(( $(date +%s)-s )) s log=$L"
grep -iE "fps|frames|time|error|fatal|gl_renderer|renderer" $L | grep -viE "fontwithface" | tail -15
