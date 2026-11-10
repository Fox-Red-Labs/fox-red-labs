<!-- experiment
id: E276
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: line found
reached: run
line: Chocolate Doom 3.1.0 with the shareware WAD runs a demo1 timedemo at 6.54 fps (one run) on the AM62L's HDMI output with no GPU, because SDL's scaler lands in Mesa llvmpipe. A wired PS4 controller reaches Linux: 11 presses and a full stick sweep were logged. Whether the game took them, and the 4:3 shape, were not seen by a person.
stop: cpu
-->
# Does Chocolate Doom run on the AM62L with no GPU, and does a wired PS4 controller reach it?

**Question.** Does Chocolate Doom run on the AM62L with no GPU, and does a wired PS4 controller reach it?

**How far it got.** It runs, slowly. `-timedemo demo1`: 5,026 game tics in 26,903 real tics, 6.54 fps, one run of 12.8 minutes (three would take 38 minutes). Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**Setup.** Chocolate Doom 3.1.0 and SDL 2.32 from Debian trixie arm64 in a container (the SDK image has Docker, no apt), shareware `doom1.wad` v1.9 (MD5 `f0cefca49926d00903cf57551d901abe`, not committed), no sound, `force_software_renderer 1`, aspect ratio correction on, 1920x1080 on HDMI.

| Video path | fps | Note |
|---|---|---|
| SDL wayland on Weston 15, pixman renderer | 6.54 | one full timedemo |
| SDL `kmsdrm` direct | about 6 | page flips counted over 17 s |
| SDL `dummy` or `kmsdrm` with the software renderer | did not start | `CreateUpscaledTexture ... max texture size 0x0`; `kmsdrm` exits with code 255 and no message |

**The line.** The limit is about 6 fps. `top -H` shows two `llvmpipe` threads at 50 to 80 percent and the Doom thread mostly waiting: SDL's KMS/DRM window needs OpenGL, and with no GPU Mesa's software renderer does the 320x200 to 1080p scaling. The Doom engine itself was not the limit. A direct frame-buffer port (doomgeneric style) would avoid it; LVGL's fbdev backend draws a full 1080p scene at 54 fps on the same cores ([HMI shootout](2026-10-24-am62l-hmi-shootout.md)).

**Expectation (before the run).** 35 fps (the engine's cap) or close, from a Doom-class game on two A53 cores at 1.25 GHz. Missed by a factor of five for the reason above.

**Controller.** The wired DualShock 4 enumerates as `Sony Computer Entertainment Wireless Controller` on `hid-generic`, `/dev/input/event1`. `evtest` during the first play window logged 15 button presses (`BTN_EAST` 11, `BTN_C` 2, `BTN_SOUTH` 1, `BTN_MODE` 1) and left-stick `ABS_X` and `ABS_Y` from 3 to 127, centered near 120. Stick up is the low end of `ABS_Y`, which is the direction Doom's joystick code reads as forward. That proves the pad reaches Linux with the right polarity; it does not prove the game acted on it.

**Not seen by a person.** The two play windows ran out before anyone watched the screen (the first ended at its timeout and showed the Weston desktop; the second saw no input). So these remain unchecked: Doom walking forward on stick up, and the 4:3 shape (`aspect_ratio_correct 1` should draw 320x200 as 1440x1080 centered; the EDID reports a 370 by 140 mm panel, so the monitor's own shape is also unknown). The bench camera returned no frame, so no picture exists. Demo recording (`-record`) did not write a file when the game was ended by a signal, so no lump was available as proof either. Check both by hand before calling a game port done.

**Not run.** The RetroArch core with the pad mapped: the timedemo used up the time. Controller-to-game proof: needs a person at the board, as above. Second and third timedemo runs: 13 minutes each, not started.

Rows: `gui_fps`, variants `chocolate-doom-3.1.0-timedemo-demo1-weston-pixman-1080p`, `chocolate-doom-3.1.0-timedemo-demo1-kmsdrm-1080p`, `retroarch-core-controller` (not_run).
