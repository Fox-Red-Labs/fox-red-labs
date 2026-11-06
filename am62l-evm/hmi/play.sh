#!/bin/sh
# 600 s Doom play with demo recording and controller event log
cd /root/doom
rm -f play1.lmp ev.log
timeout 500 evtest /dev/input/event1 > ev.log 2>&1 &
docker exec -d -e SDL_VIDEODRIVER=wayland -e SDL_RENDER_DRIVER=software -e XDG_RUNTIME_DIR=/root/wl -e WAYLAND_DISPLAY=wl0 -e HOME=/root/doom doom sh -c "cd /root/doom && timeout 480 /usr/games/chocolate-doom -iwad /root/doom/doom1.wad -config /root/doom/cd.cfg -nosound -nomusic -warp 1 1 -skill 1 > play.log 2>&1"
