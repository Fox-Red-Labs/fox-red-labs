#!/bin/sh
# Inside frl-show, Xorg on :0: show the Windows guest's screen full screen on the HDMI panel. QEMU serves VNC
# only on a Unix socket (root-only directory); the viewer opens that socket directly, with no TCP listener.
# Usage: sh vnc_panel.sh        stop: pkill -x xtigervncviewer
export DISPLAY=:0
pkill -x xtigervncviewer; pkill -f "socat TCP-LISTEN:5951"; sleep 1
xtigervncviewer -FullScreen -SecurityTypes=None -RemoteResize=0 /root/frl/kvm/win/vnc.sock > /root/frl/showcase/out/vnc.log 2>&1 &
