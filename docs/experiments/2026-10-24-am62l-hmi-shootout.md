<!-- experiment
id: E275
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: works
reached: run
line: With no GPU, LVGL 9.3 draws a fixed 1080p scene at 54 fps through fbdev (30 percent of both cores) and 18 fps through its DRM backend (46 percent); Slint 1.17's software renderer reaches about 42 fps (47 percent). One run set, three 30 s runs each; no bench-camera picture.
stop: Qt Quick software and the camera picture were not done.
-->
# How fast can the AM62L draw a 1080p HMI with no GPU?

**Short answer.** Median of three 30 s runs per row, 1920x1080 at 60 Hz on the HDMI monitor, both A53 cores at 1.25 GHz. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

| Toolkit and backend | fps | CPU, percent of both cores | RAM (resident set) |
|---|---|---|---|
| LVGL 9.3.0, fbdev (`/dev/fb0`) | 54.1 | 29.8 | 2.0 MB |
| LVGL 9.3.0, DRM (`/dev/dri/card0`, dumb buffers) | 18.0 | 45.8 | 1.6 MB |
| Slint 1.17.1, `linuxkms-software` | 41.9 | 46.7 | 24.6 MB |

One core would read twice the CPU figure: the DRM and Slint rows keep one core about 92 percent busy, so they are limited by one rendering thread. The fbdev row has no vsync and is paced by the 16 ms refresh timer, so it is not CPU bound.

**Expectation (before the run).** 30 to 60 fps for LVGL fbdev, from the 60 Hz refresh and the 1.25 GHz A53 filling about 2 million pixels a frame; no number existed for the other two. fbdev landed inside it. The DRM backend came in at a third of fbdev, which was not expected.

**The scene.** Same in both toolkits (`am62l-evm/hmi/main.c`, `am62l-evm/hmi/scene.slint`): 12 bars animated left to right (1.5 to 3.5 s ease-in-out), 12 counters and one large counter updated every 16 ms, dark background. The two are written separately, so they are the same scene, not the same draw calls.

**Method.** `am62l-evm/hmi/run_hmi.sh`: `ti-lvgl-demo` stopped, no containers, wait for a 1-minute load under 0.3 (readings 0.26 to 0.33), 5 s warm-up, 30 s window. CPU is the process's utime plus stime over the window; RAM is VmRSS (the frame buffer mapping is not in it). LVGL fps is counted at its refresh-ready event. Slint prints no rate on this build, so page flips were counted with `strace --seccomp-bpf -e trace=ioctl`; the same method on the LVGL DRM backend read 17.1 against 18 counted inside LVGL, so the Slint figure is probably 5 percent low.

**Unexplained.** LVGL fbdev renders three times as fast as LVGL DRM on the same CPU. Both use the same software draw; the DRM path commits atomically to a dumb buffer. Not isolated (one guess, a slow uncached mapping, is untested).

**Not run.**
- Qt Quick software renderer: Slint covered the software-toolkit test, and Qt is not on the image or in the Debian container used here.
- A picture of each screen was not taken. The scene source is in the repo.
- Slint with the Skia (GL) renderer: the board has no GPU, so only the software renderer was measured.

Rows: `gui_fps` and `host_cpu_load`, variants `lvgl-9.3-fbdev-1080p`, `lvgl-9.3-drm-1080p`, `slint-1.17.1-linuxkms-software-1080p`.
