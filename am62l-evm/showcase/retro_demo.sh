#!/bin/sh
# Inside frl-show, Xorg on :0: RetroArch 1.20 (Debian) with the Gambatte Game Boy core running Tobu Tobu Girl,
# a freely licensed homebrew game (code MIT, assets CC BY 4.0, Tangram Games), full screen on the HDMI panel.
# The HDMI mode is 640x480 while it runs (the monitor scales it). The PS4 pad drives it (udev joypad driver, autoconfig below). --max-frames bounds the run; at full speed
# N frames take N / 59.73 s, so the wall time shows whether the core keeps up.
# Usage: sh retro_demo.sh [frames=6000]
N=${1:-6000}; O=/root/frl/showcase/out; R=/root/frl/showcase/roms; mkdir -p $O $R /root/.config/retroarch/autoconfig/udev
export DISPLAY=:0
[ -f $R/tobu.gb ] || curl -fsSL --max-time 60 -o $R/tobu.gb https://raw.githubusercontent.com/gbdev/database/master/entries/tobutobugirl/tobu.gb
echo "5d3871cae77db2287807e8914fd21f76203e73aa3fec20955489d45cb4571d8f  $R/tobu.gb" | sha256sum -c - || exit 1
cat > /root/.config/retroarch/autoconfig/udev/ds4-hid-playstation.cfg <<'CFG'
input_driver = "udev"
input_device = "Sony Computer Entertainment Wireless Controller"
input_b_btn = "0"
input_a_btn = "1"
input_y_btn = "3"
input_x_btn = "2"
input_l_btn = "4"
input_r_btn = "5"
input_select_btn = "8"
input_start_btn = "9"
input_menu_toggle_btn = "10"
input_up_btn = "h0up"
input_down_btn = "h0down"
input_left_btn = "h0left"
input_right_btn = "h0right"
input_l_x_plus_axis = "+0"
input_l_x_minus_axis = "-0"
input_l_y_plus_axis = "+1"
input_l_y_minus_axis = "-1"
CFG
cat > $O/retroarch-show.cfg <<'CFG'
video_driver = "gl"
video_fullscreen = "true"
video_windowed_fullscreen = "true"
video_vsync = "true"
video_smooth = "false"
video_scale_integer = "true"
audio_driver = "null"
input_driver = "x"
input_joypad_driver = "udev"
joypad_autoconfig_dir = "/root/.config/retroarch/autoconfig"
menu_driver = "rgui"
fps_show = "true"
pause_nonactive = "false"
CFG
xrandr --output HDMI-1 --mode 640x480   # 3x integer scale of 160x144 fits; at 1080p llvmpipe managed 10.4 fps
(sleep 15; xdotool key Return; sleep 3; xdotool key Return; sleep 3; xdotool key Return; sleep 20; import -window root $O/retroarch.png) &
s=$(date +%s.%N)
retroarch --config $O/retroarch-show.cfg -L /usr/lib/aarch64-linux-gnu/libretro/gambatte_libretro.so \
  --max-frames=$N --verbose $R/tobu.gb > $O/retroarch.log 2>&1
e=$(date +%s.%N)
xrandr --output HDMI-1 --mode 1920x1080
awk -v s=$s -v e=$e -v n=$N 'BEGIN{w=e-s; printf "frames=%d wall_s=%.1f fps=%.1f full_speed_s=%.1f\n", n, w, n/w, n/59.73}' 
grep -iE "joypad|autoconf|Wireless Controller|error" $O/retroarch.log | head -5
