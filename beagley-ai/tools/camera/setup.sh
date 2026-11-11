#!/bin/sh
# Set up a USB webcam on the BeagleY-AI from a Mac. Usage: beagley-ai/tools/camera/setup.sh <board address>
# Installs the udev rule, re-triggers it for a camera already plugged in, and prints the camera's formats.
set -e
B=${1:?board address}; D=$(cd "$(dirname "$0")" && pwd)
scp -q "$D/99-usb-cam.rules" root@"$B":/etc/udev/rules.d/
ssh root@"$B" 'udevadm control --reload && udevadm trigger --subsystem-match=video4linux --action=add && sleep 1
  ls -l /dev/video-usb-cam0 || { echo "NO CAMERA: check lsusb; unplug and replug it"; lsusb; exit 1; }
  v4l2-ctl -d /dev/video-usb-cam0 --info | sed -n "s/^\tCard type *: /camera: /p"
  v4l2-ctl -d /dev/video-usb-cam0 -C power_line_frequency
  v4l2-ctl -d /dev/video-usb-cam0 --list-formats | grep -o "'"'"'[A-Z0-9]*'"'"'" | tr "\n" " "; echo'
