#!/bin/sh
# Inside frl-show, Xorg on :0: SuperTuxKart 1.5 (native arm64 release build) on the HDMI panel. The HDMI mode
# is switched to 640x480 so the monitor scales the picture to full screen; the AI drives a 4-kart race and
# --profile-time makes STK print its frame statistics when it quits.
# Usage: sh stk_demo.sh [seconds=110]
T=${1:-110}; O=/root/frl/showcase/out; mkdir -p $O
D=/root/frl/games/stk/a/SuperTuxKart-1.5-linux-arm64
export DISPLAY=:0 HOME=/root/frl/games/stk/home-arm64 SUPERTUXKART_DATADIR=$D SUPERTUXKART_ASSETS_DIR=$D/data/
xrandr --output HDMI-1 --mode 640x480
cd $D; s=$(date +%s)
(sleep 45; import -window root $O/stk.png) &
./run_game.sh --screensize=640x480 --fullscreen --profile-time=$T --track=lighthouse --numkarts=4 --no-start-screen > $O/stk.log 2>&1
echo "rc=$? wall=$(( $(date +%s)-s ))"
xrandr --output HDMI-1 --mode 1920x1080
grep -iE "fps|frames|renderer" $O/stk.log | grep -viE fontwithface | tail -6
