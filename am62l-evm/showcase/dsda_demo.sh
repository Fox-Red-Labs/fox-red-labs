#!/bin/sh
# Inside frl-show, Xorg on :0: dsda-doom 0.29 (Debian's prboom-plus package) with the shareware DOOM1.WAD,
# full screen at a 640x480 HDMI mode, OpenGL on llvmpipe, playing its built-in demos (attract mode) for T s.
# Usage: sh dsda_demo.sh [seconds=120]
T=${1:-120}; O=/root/frl/showcase/out; mkdir -p $O
export DISPLAY=:0 HOME=/root/frl/showcase/dsda-home; mkdir -p $HOME
xrandr --output HDMI-1 --mode 640x480
(sleep 40; import -window root $O/dsda.png) &
timeout $T /usr/games/dsda-doom -iwad /root/frl/doom/wad-cache/DOOM1.WAD -width 640 -height 480 -fullscreen -nosound -nomusic > $O/dsda.log 2>&1
xrandr --output HDMI-1 --mode 1920x1080
