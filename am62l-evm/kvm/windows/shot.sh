#!/bin/bash
# shot.sh NAME: screendump of the guest's ramfb into /w/win/shots/NAME.ppm (runs inside frl-kvm)
# key.sh-style use: shot.sh --key ret  sends one key through the monitor
W=/w/win; mkdir -p $W/shots
if [ "$1" = --key ]; then echo "sendkey $2" | socat - UNIX-CONNECT:$W/mon.sock >/dev/null; exit; fi
echo "screendump $W/shots/$1.ppm" | socat - UNIX-CONNECT:$W/mon.sock >/dev/null
