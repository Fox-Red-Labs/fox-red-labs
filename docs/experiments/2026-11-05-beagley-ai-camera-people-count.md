<!-- experiment
id: E294
board: BeagleY-AI
date: 2026-11-05
status: works
reached: verified
line: TI's people-count camera demo ran its detector on the C7x at 28.0 to 29.4 fps from a USB webcam on the BeagleY-AI (AM67A), and 5 of 5 starts held 60 s on a rated 5 V 3 A supply; on the earlier wall supply 0 of 5 held 15 s; no person has been counted yet, because the camera faced the bench (count 0)
stop: none
-->
# Does TI's people-count camera demo run on a USB webcam at 30 fps on the BeagleY-AI (AM67A)?

Terms: [docs/vocab.md](../vocab.md), BeagleY-AI FAQ. **C7x** is the AM67A's AI accelerator core. 1 kB = 1,024 bytes. Setup: BeagleY-AI 4 GB, BeagleBoard Edge AI image 11.00.00.08 (Linux 6.12.33), an Insta360 Link 2 Pro webcam at 720p, TI's `app_edgeai.py` detection demo.

**Short answer.** Yes for the detector: on a rated 5 V 3 A USB-C supply it ran at 28.0 to 29.4 fps and the board stayed up through five 60 s starts. No person has been counted yet: the on-screen count was 0 because the camera faced the bench, not a person. On the earlier wall supply the board rebooted; the supply is the suspected cause, not a measured one.

**Question.** Does TI's camera demo (`app_edgeai.py`) detect and count people from the USB webcam at the camera's 30 fps, and does it stay up?

**Expectation, written first.** 28 to 30 fps (the camera's rate). We also expected the C7x to be about 25 percent busy, as on the recorded clip; that was not measured on the camera runs.

**What we tried.** Both rounds used `beagley-ai/bench/camdetect/camdetect.py <board address> camera`, a script in this repo that copies TI's app and its settings to the board over SSH, starts it with the environment variable `SOC=j722s` (the chip name TI's app needs; without it the model runs on the Arm cores at 1.3 fps), and reads the MJPEG stream (a video stream of JPEG pictures) back on the Mac. On 2026-11-05, wall supply: five starts; then probes of camera alone at 720p for 15 s (30.00 fps, held), four Arm cores busy plus camera for 20 s (held), the pose model alone for 60 runs (held), and Pop the bubbles (P63, this repo's body-pose game) with the camera and no display for 20 s (held). The failures needed TI's detection app, or the game's browser stream, which encodes each frame as JPEG, on top of the camera. On 2026-11-07, rated supply: five starts of the same command, each read from the Mac for 60 s with the board's uptime checked after each. The camera sat on the board's own USB hub, not an external powered hub, and did not drop. Board temperature was 78 to 83 C without a fan. Logs: `beagley-ai/bench/camera-power/probe-2026-11-05.log`, `e294-five-starts-2026-11-07.log`, `supply-check-2026-11-07.log`; a frame: `camera-frame-e294-start1-2026-11-07.jpg`.

| Date | Supply | Starts held | Detector fps | Result |
|---|---|---|---|---|
| 2026-11-04 | Wall supply, rating unread; recorded clip, no camera | Clip start held | 30.0 | Clip works; a clip start dropped once in two on 2026-11-05 |
| 2026-11-05 | Wall supply, rating unread | 0 of 5 held 15 s | 29.9 for the first 5 s | Four starts ended with the board off Ethernet and rebooting 5 to 15 s in; the fifth hung with an empty log |
| 2026-11-07 | Rated 5 V 3 A USB-C | 5 of 5 held 60 s | 29.4, 28.9, 28.9, 28.8, 28.0 | Board stayed up (uptime 1 day 10 h before and after) |

**How far it got.** Run: five starts of 60 s each at 28.0 to 29.4 fps, board up throughout, on the rated supply. The count itself was only seen as 0.

**The line.** None reached on the rated supply. The earlier reboots happened on a supply of unread rating and did not recur on a rated one; the cause is suspected, not measured. The supply's rating is the bench owner's statement: the label was not photographed, and there is no power meter on the port.

**Next moves.** Put a person in view and read the count against the number of people present. Photograph the supply label. Results: `data/results/beagley-ai.csv`, rows `e294-people-count-usb-webcam-*`.

**Live from a network camera.** On 2026-11-08 the same detector ran on video streamed over the LAN from the webcam, which stays on a MacBook (the board has no camera on it for this run). Setup: `beagley-ai/bench/camera-lan/mac_stream.sh` runs mediamtx on the Mac's LAN address only and publishes the camera to it with ffmpeg (1280x720, 30 fps, H.264 at 4 Mbit/s); `RTSP_HOST=<Mac address> camdetect.py <board address> rtsp` points TI's app at that RTSP address. Expected, written first (`beagley-ai/bench/camera-lan/expectations.txt`): 28 to 30 fps, a count of 0 on the empty scene, and a count above 0 on the recorded clip.

| What | Expected | Measured | Source |
|---|---|---|---|
| Detector fps, network camera, 60 s | 28 to 30 | 29.7 (1,782 frames in 59.9 s) | `results-2026-11-08.log` |
| People counted, empty scene | 0 | 0 (frame `rtsp-empty-scene-2026-11-08.jpg`) | same |
| C7x busy, network camera | about 25 percent | 24 percent, about 8 ms a frame at 29 fps (derived from the overlay; the app prints no per-frame time) | same |
| Detector fps, recorded clip, 30 s | 30 | 30.0 | same |
| People counted, clip | above 0 | 3 in the saved frame (`clip-people-2026-11-08.jpg`) | same |

The count of 0 is an empty-scene baseline: nobody was in view, so counting real people from the network camera is still untested. End-to-end latency was not measured: the pipeline does not report it, and the camera view had nothing to time against. Rows: `e294-people-count-network-camera-rtsp-*` and `e294-people-count-clip-*-rerun`. One failure: `pkill` on the app while it read RTSP left it stuck in the video decoder and the next start failed; a reboot cleared it (`data/recovery.json`); stopping with SIGINT and a 6 s wait did not hang.

**Limits.** One board, two supplies, no serial console, no power meter and no stored log on the board, so the supply is the likely cause of the earlier reboots, not a measured one. The 2026-11-07 boot log shows `tps65219 2-0030: Error IRQ trap undervoltage for LDO2` and for `BUCK2` at about 1.2 s, before any camera start, on the rated supply too, and no reboot followed; what they mean for the rails was not examined. The camera's view held no person, so counting real people is untested.
