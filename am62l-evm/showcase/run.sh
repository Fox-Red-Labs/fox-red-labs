#!/bin/sh
# On the board (host): loop a showcase on the HDMI panel, about 2 minutes per item, games first:
#   SuperTuxKart at 640x480 (its slot is 7 min longer: the race starts about 7 min after launch),
#   dsda-doom on OpenGL at 640x480, RetroArch with a homebrew Game Boy game, Doom on the framebuffer,
#   then Android 12 (Redroid through scrcpy) and TI's LVGL demo.
# Needs: kernel 6.18.38-ti-kvm (binder, for Redroid), the frl-show container (setup.sh, build_scrcpy.sh),
# the frl-redroid container booted once (redroid_boot.sh; paused between Android turns).
# Usage: nohup sh run.sh [seconds per item=120] > /root/frl/showcase/out/loop.log 2>&1 &
# Stop: sh run.sh stop   (kills the loop and its items, then starts Doom alone again as before)
T=${1:-120}; S=/root/frl/showcase; O=$S/out; mkdir -p $O
X="docker exec frl-show"
x_start() { pgrep -x Xorg >/dev/null || { $X sh -c "Xorg :0 vt7 -config $S/xorg.conf -nolisten tcp -novtswitch > $O/xorg.log 2>&1 &"; sleep 5; $X sh -c "DISPLAY=:0 openbox >/dev/null 2>&1 &"; sleep 1; }; }
x_stop() { $X pkill -x openbox; $X pkill -x Xorg; sleep 3; }
doom_start() { cd /root/frl/doom && nohup ./build/fb/doom_fb -iwad wad-cache/DOOM1.WAD -nosound > /dev/null 2>&1 & }
log() { echo "$(date +%H:%M:%S) $*"; }
if [ "$1" = stop ]; then
  [ -f $O/run.pid ] && kill $(cat $O/run.pid) 2>/dev/null; rm -f $O/run.pid; pkill -f doom_fb; systemctl stop ti-lvgl-demo
  $X pkill -x scrcpy; $X pkill -f supertuxkart; $X pkill -x retroarch; x_stop
  docker pause frl-redroid 2>/dev/null; doom_start; exit 0
fi
echo $$ > $O/run.pid
pkill -f doom_fb; systemctl stop ti-lvgl-demo
docker ps -q -f name=frl-redroid | grep -q . || sh /root/frl/redroid/redroid_boot.sh 300 > $O/redroid-boot.log 2>&1
docker pause frl-redroid 2>/dev/null
while true; do
  x_start
  log "stk start"; $X timeout $(( T + 420 )) sh $S/stk_demo.sh 900 | sed 's/^/  /'; $X pkill -f supertuxkart; $X xrandr -d :0 --output HDMI-1 --mode 1920x1080
  log "dsda-doom start"; $X sh $S/dsda_demo.sh $T
  log "retroarch start"; $X sh $S/retro_demo.sh $(( T * 60 )) | sed 's/^/  /'
  log "android start"; docker unpause frl-redroid; $X sh $S/android_demo.sh $T | sed 's/^/  /'; docker pause frl-redroid
  x_stop
  log "doom start"; doom_start; sleep $T; pkill -f doom_fb; sleep 1
  log "lvgl start"; systemctl start ti-lvgl-demo; sleep $T; systemctl stop ti-lvgl-demo; sleep 2
done
