#!/bin/sh
# Inside frl-show, with Xorg on :0 and the frl-redroid container booted (redroid_boot.sh):
# Android 12 on the HDMI panel through scrcpy, landscape 1280x720, then the launcher and the Settings app
# scrolled by injected swipes. scrcpy prints the frame rate it shows each second.
# Usage: sh android_demo.sh [seconds=110]
T=${1:-110}; O=/root/frl/showcase/out; mkdir -p $O
export DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/xdg; mkdir -p -m 700 $XDG_RUNTIME_DIR
A="adb -s 127.0.0.1:5555"
$A connect 127.0.0.1:5555 >/dev/null 2>&1
$A shell wm size 1280x720; $A shell wm density 160
$A shell input keyevent KEYCODE_HOME
pkill -x scrcpy; sleep 1
scrcpy -s 127.0.0.1:5555 --fullscreen --max-fps=30 --video-bit-rate=4M --print-fps --no-audio > $O/scrcpy.log 2>&1 &
sleep 10; import -window root $O/android-launcher.png
$A shell am start -a android.settings.SETTINGS >/dev/null; sleep 4
s=$(date +%s)
while [ $(( $(date +%s)-s )) -lt $(( T-20 )) ]; do
  $A shell input swipe 640 600 640 150 500; $A shell input swipe 640 150 640 600 500
done
import -window root $O/android-settings.png
$A shell input keyevent KEYCODE_HOME
pkill -x scrcpy
grep -oE "[0-9]+ fps" $O/scrcpy.log | awk '{n++; s+=$1; if($1>m)m=$1} END{if(n) printf "scrcpy_fps_mean=%.1f max=%d samples=%d\n", s/n, m, n}'
