# Doom on the TMDS62LEVM (AM62L) from a Linux framebuffer, no GPU

`dg_fbdev.c` is a doomgeneric platform layer that writes the engine's 320x200 frame straight into `/dev/fb0`
(32 bpp), whole-number scaled and centered, and reads one evdev game pad. No SDL, no compositor, no OpenGL.
Terms: [vocab](../../../docs/vocab.md), [AM62L FAQ](../../../am62l-evm/FAQ.md). **fps** is frames per second; **evdev** is Linux's input-device interface.

On the TMDS62LEVM (AM62L, two A53 cores at 1.25 GHz, 1920x1080 HDMI) `-timedemo demo1` runs at 186 to 193 fps at
1280x1000 (scale 4x5), 241 fps at 960x800 and 413 fps unscaled, against 6.5 fps for Chocolate Doom through
Weston and Mesa llvmpipe. Numbers: `data/results/am62l-evm.csv`, variants `doomgeneric-fbdev-*`.

Build and run on the board (the image has no compiler, so the build runs in a Debian trixie arm64 container):

```sh
./fetch.sh                                  # on the Mac, in ports/doom: engine at the pinned commit, patched, and the WAD
# copy srclist.sh, linux-fb/, src-upstream/doomgeneric/ and wad-cache/DOOM1.WAD to /root/doom on the board
sh linux-fb/build.sh /root/doom         # about 75 s on the board's two cores
systemctl stop ti-lvgl-demo                 # the display demo owns the screen
./build/fb/doom_fb -iwad wad-cache/DOOM1.WAD -timedemo demo1 -nosound
DOOM_FPS_EVERY=5 ./build/fb/doom_fb -iwad wad-cache/DOOM1.WAD -nosound      # play with the pad
pkill doom_fb; systemctl start ti-lvgl-demo
```

Pad (console layout, PS4 DualShock 4 names):

| Button | Action |
|---|---|
| Left stick or d-pad | Move forward and back, turn left and right |
| Cross | Fire; Enter and yes in the menus |
| Square | Use: open doors, press switches |
| Circle (hold) | Run |
| Triangle | Next weapon |
| L1, R1 | Strafe left, strafe right |
| R2 | Fire |
| Options | Menu (Escape) |

If you see the buttons doing the wrong things on a DualShock 4, check which driver owns the pad: with
`CONFIG_HID_PLAYSTATION` and `CONFIG_HID_SONY` unset (as on the AM62L SDK image), `hid-generic` reports the buttons in
HID order (square 304 `BTN_SOUTH`, cross 305, circle 306 `BTN_C`, triangle 307, L1 308, R1 309 `BTN_Z`, R2 311,
Options 313), so the standard names are off by one. `dg_fbdev.c` reads a pad that reports `BTN_C` or `BTN_Z` with
that table and prints `hid-generic DualShock 4 button table` at start; any other pad uses the standard names.
`DOOM_SCALE=3x4` picks another whole-number scale.

Limits: no vsync, so a fast turn can tear; no sound. 320x200 shown 4x5 is 1280x1000, aspect 1.28 against the
4:3 (1.33) a CRT showed; the nearest whole-number pair that fits 1080 lines.
