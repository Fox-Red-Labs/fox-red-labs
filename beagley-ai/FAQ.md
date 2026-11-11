# BeagleY-AI (TI AM67A) from a Mac: errors, causes and fixes

Problems hit bringing up one BeagleY-AI 4 GB from a Mac on 2026-11-03. Each answer starts with the fix and quotes the error text where a log kept it. Versions: BeagleBoard Edge AI image 11.00.00.08 (built 2025-09-06), Linux 6.12.33, GCC 13.3.0, onnxruntime 1.15.0, GStreamer and FFmpeg as shipped on that image. 1 kB = 1,024 bytes.

Terms: [docs/vocab.md](../docs/vocab.md). **C7x** is the AM67A's DSP core, **TIDL** TI's runtime that runs a network on it, **Wave5** the video codec block. Cause tags: Setup or handling (how it was plugged in or set); Tool or docs (a tool or missing instruction); Third-party behavior (macOS, the kernel driver, a container image); Chip limit; Unknown (not found; the lead is named).

## Getting started

### The Mac will not write to the card: "refusing disk4: not removable", or the write ends with an error

Use the current `beagley-ai/tools/write-card.sh`: it accepts a card in the Mac's built-in SD slot, which lists as an internal, fixed disk, while still refusing the Mac's system disk and anything outside 16 to 512 GB. It unpacks the image to a file and pads it to whole 512-byte sectors before the raw write, because a raw write to `/dev/rdiskN` ending in a partial sector fails. It needs about 11 GB free in the work folder (third argument). The script's checks were run against the system disk and a 2 TB USB drive (both refused); a full card write with this version has not been run. No log was kept of the failed attempts; the script is `beagley-ai/tools/write-card.sh`.

### The board is powered from my computer's USB-C port but never shows up as a network adapter

Reach the board over Ethernet. On the Edge AI image the USB-C port carries power only: there is no USB gadget, so no serial or network device appears on the Mac. Find the address in the router's client list (the board's host name is `j722s-evm`). Source: `beagley-ai/FIRST-BOOT.log`.

## Models on the C7x

### A model load never returns, and the next models hang too

Reboot the board. Run one model per process with a timeout, as `beagley-ai/bench/zoo/zoo_bench.py` does with `ONLY=<model folder> timeout 300 python3 zoo_bench.py 30`, so a hung load costs one model and not the session. Seen once on 2026-11-03; no log of the hang was kept and the cause is not known.

### Pose takes 43 ms; the SK-TDA4VM's numbers suggested 25 to 30 ms

Nothing is wrong; the 25 to 30 ms was our guess, scaled from the SK-TDA4VM's 13.76 ms and the 4 against 8 TOPS ratio. Both C7x cores report their 1.0 GHz clock (`k3conf dump processor`), and every one of the model's 274 nodes runs on the accelerator. TI's AM67A datasheet table gives 8.63 ms for YOLOX-nano and 4.94 ms for RegNetX; this board measured 10.15 and 5.95 ms with the Python timing used here, about 19 percent more, which is near TI's published speed but not equal to it; the pose model is not in that table, and about 36 ms is the figure TI's method would give (computed, not run). The SK-TDA4VM (8 TOPS) ran the same model in 13.76 ms. Source: `beagley-ai/bench/zoo/clocks-2026-11-04.log`, `beagley-ai/bench/zoo/zoo_tidl.log`.

## Video decode and encode

### `ffmpeg -c:v h264_v4l2m2m` encode never finishes; `dmesg` prints `wave5_vpu_firmware_command_queue_error_check: result not ready: 0x800`

Use GStreamer for hardware encode: `v4l2h264enc` ran at 110.1 fps on a raw 1088p file. FFmpeg's `h264_v4l2m2m` encoder did not finish in over 100 s, and on a second try waited in `poll` for over 3.5 minutes, ignored `timeout`'s SIGTERM and wrote 48 bytes; `kill -9` ends it and the codec still works afterwards. The cause is in the FFmpeg wrapper for the Wave5 driver. Source: `beagley-ai/bench/video/ffmpeg-encode-hang-dmesg.log`, `beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`.

### FFmpeg hardware decode gives 3 frames and then stops when the output is scaled and piped

Decode with GStreamer (`v4l2h264dec`) when the frames go to a program. `ffmpeg -c:v h264_v4l2m2m -i clip.h264 -f null -` decodes the whole clip (1.1 s for 300 frames at 1280x768), but with `-vf scale=224:224 -pix_fmt rgb24 -f rawvideo -` it delivered 441 kB (3 frames of 224x224x3) and then waited about 20 s with no error text. Source: `beagley-ai/bench/seam/run-2026-11-03.log`.

### GStreamer `v4l2h264dec ! videoconvert` to a pipe runs at 6 fps, or seems to hang

Scale the decoded frame before converting it: `v4l2h264dec ! videoscale n-threads=4 ! video/x-raw,width=224,height=224 ! videoconvert ! video/x-raw,format=RGB`. That ran 300 frames of the 1280x768 clip in 3.92 s (76.5 fps, 5 of 5 runs), and the seam app then ran at 61.3 fps with the classifier on the C7x. The other order, `videoconvert` on the full frame first, ran at 6.5 fps: the likely cause is that the CPU reads every pixel of the decoder's output buffers and those reads are slow (suspected; not measured on its own). At that rate 100 frames plus a model load overran the 90 s timeouts, which is what the earlier "failed" runs were. Leave `capture-io-mode` at its default and do not set `n-threads` on `videoconvert` after the decoder: both crash `gst-launch-1.0` with `Caught SIGSEGV`, after which it waits for a debugger instead of exiting. Source: `beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`.

### `dmesg` prints `wave5_vpu_firmware_command_queue_error_check: result not ready: 0x800`

Ignore it on its own: the driver prints it once at the end of every decode, including runs that finish with every frame. Source: `beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`.

### Decode and encode chained run at 35 fps though each alone runs above 100

Give the encoder the decoder's buffers: `v4l2h264dec ! v4l2h264enc output-io-mode=dmabuf-import`. That transcoded 600 frames at 1920x1088 in 4.44 s (135.1 fps, 3 of 3 runs) against 17.09 s (35.1 fps) with the default, which copies every frame through the CPU. Source: `beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`.

## The USB webcam, the camera demo and power

### The webcam drops off USB: `Non-zero status (-71) in video completion handler`, then `USB disconnect`

Power the board from a 5 V 3 A USB-C supply (BeagleBoard's requirement; it has no USB-PD and warns against unpowered hubs) and if the camera still drops, give it a powered USB hub (Insta360 lists the Link 2 Pro at 5 V 1 A); on a rated 5 V 3 A supply it held on the board's own hub in five 60 s starts. On a board fed from a hub port, the Insta360 Link 2 Pro dropped off at 1080p 7 of 7 times (MJPEG and H.264, with USB autosuspend off too) and after a gimbal move; 720p held 3 of 3 with the board idle but dropped once the board ran a Frigate container, and so did a microphone recording. It comes back about 5 s later with pan, tilt and zoom reset. Software settings did not change it; that it is the supply is the likely cause, not measured (no meter on the port). Source: `beagley-ai/bench/camera-usb-drops-2026-11-03.log`.

### Apps cannot find `/dev/video-usb-cam0` after the webcam is plugged in

TI's `setup_cameras.sh` makes that name only at boot. Install the udev rule from a Mac with `beagley-ai/tools/camera/setup.sh <board address>`: it names the first UVC camera `/dev/video-usb-cam0` on every plug-in and sets its mains frequency to 60 Hz. Source: `beagley-ai/tools/camera/99-usb-cam.rules`.

### TI's Edge AI app runs at 1 fps with `c7x_1: 0%`; its log says `SOC env var not specified. Defaulting target to arm`

Set `SOC=j722s` before starting `app_edgeai.py`. A login shell sets it from `/etc/profile`; a command run as `ssh root@<board> "..."` does not, and the app then runs the model on the Arm cores (737 to 809 ms a frame, 1.3 fps) and decodes in software. With `SOC` set, the people-count demo ran at 30.0 fps with the C7x 27 percent busy. `beagley-ai/bench/camdetect/board_start.sh` sets it. Also start the app with `-n` when it has no terminal (`_curses.error: setupterm: could not find terminal`). Source: `beagley-ai/bench/camdetect/clip-run-2026-11-04.log`.

### TI's detection demo crashes: `IndexError: index 5 is out of bounds for axis 1 with size 1` in `post_process.py`

Apply `beagley-ai/bench/camdetect/people_count.patch`: `np.squeeze` in `PostProcessDetection` also removes the box axis when a frame holds exactly one detection. The patch reshapes each output to one row per detection. Source: `beagley-ai/bench/camdetect/clip-run-2026-11-04.log`.

### The board drops off Ethernet and reboots a few seconds after TI's camera demo starts

Power the board from a rated 5 V 3 A USB-C supply and start the demo again. On a wall supply of unread rating, 4 of 5 starts ended with the board off the network and rebooting 5 to 15 s in (the fifth hung with an empty log; the first start streamed 29.9 fps for 5 s). On a rated 5 V 3 A supply, 5 of 5 starts held 60 s at 28.0 to 29.4 fps and the board stayed up. The cause is suspected to be the supply, not measured: the board has no console, no power meter and keeps no log. The boot log also prints `tps65219 2-0030: Error IRQ trap undervoltage for LDO2` and `BUCK2`, on the rated supply as well, with no reboot after them. Source: `beagley-ai/bench/camera-power/probe-2026-11-05.log`, `beagley-ai/bench/camera-power/e294-five-starts-2026-11-07.log`, `beagley-ai/bench/camera-power/supply-check-2026-11-07.log`.

### How do I check that my USB-C supply is 5 V 3 A?

Read the output line on the supply's label (for example `5V 3A`, or `5V 3A` among several USB-PD steps) and use a supply with a single output, since many dual-port chargers drop each port to 1.5 A when both are in use. The board has no USB-PD, so it needs a supply that offers 5 V at 3 A without negotiating. Nothing on the board reports the supply's rating; `dmesg | grep tps65219` shows undervoltage lines, which do not name the supply behind them. For the 2026-11-07 runs the supply's rating is the bench owner's statement; the label was not photographed.

### TI's people-count demo shows a count of 0 with a person in the room

Aim the camera at the person and check the stream's picture first. In the 2026-11-07 runs the Insta360 Link 2 Pro faced the bench, so the on-screen count was 0 while the detector ran at 28.0 to 29.4 fps; no person has been counted with this camera yet. Save a frame (`beagley-ai/bench/camdetect/grab_stream.py`) to see what the camera sees. Source: `beagley-ai/bench/camera-power/camera-frame-e294-start1-2026-11-07.jpg`.

### Do I need a powered USB hub for the webcam?

Not on a rated 5 V 3 A supply at 720p: the Insta360 Link 2 Pro on the board's own USB hub did not drop in five 60 s starts of TI's camera demo, or in a 60 s run of Pop the bubbles. The earlier advice came from a board fed through a USB hub port, where the camera dropped at 1080p and under load (see the webcam entry above). Try the board's own hub first on a rated supply; add a powered hub if the camera still drops. Source: `beagley-ai/bench/camera-power/e294-five-starts-2026-11-07.log`, `beagley-ai/bench/camera-power/p63-browser-60s-2026-11-07.log`.

### The camera demo's start script prints `STREAM FAILED`

Run `pkill -f server.js` on the board, then start again. A `node server.js` left from an earlier start keeps port 8081, and the start script's own `pkill` pattern (`remote_streaming/server.js`) does not match that command line. Source: `beagley-ai/bench/camdetect/board_start.sh`.

## Containers, programs and the operating system

### Pi-hole starts but DNS queries time out: `failed to create listening socket for port 53: Address in use`

`systemd-resolved` holds 127.0.0.53:53. Start the container with `FTLCONF_dns_listeningMode=BIND` and `dns.interface=eth0`; `dig @<board address> doubleclick.net` then answered `0.0.0.0`. Source: `beagley-ai/bench/docker/pihole-attempt1-LOCAL-mode-failed.log`, `beagley-ai/bench/docker/pihole.log`.

### The board reads 80 to 87 degrees C during long runs

The 4-pin fan connector was empty. Fit a fan for runs longer than a few minutes. No run was made with a fan and throttling was not checked, so any number here may be low. No temperature log was kept; the readings were taken by hand during the runs.

### `cannot find -lc` when linking with `-static`

The image has no `libc.a`. Link dynamically; the stripped portable app is 66.1 kB. Source: `beagley-ai/bench/portability/three-ways-run.log`.

### A program dies with `Illegal instruction`

The Cortex-A53 has no ARMv8.1 atomics. Preload the trap library from `am62l-evm/steam/lse_trap/` (built on this board with `gcc -O2 -march=armv8-a -mno-outline-atomics -fPIC -shared -o lse_trap.so lse_trap.c -ldl`; its two tests print PASS). Source: `beagley-ai/bench/portability/lse-trap-run.log`.

### `/dev/kvm` and `/dev/fb0` do not exist

The stock kernel is built without KVM (`# CONFIG_KVM is not set`), so a virtual machine needs a rebuilt kernel; Android's binder and `binfmt_misc` are off as well. `/dev/fb0` appears only when a display is attached: the kernel has framebuffer emulation on top of DRM (`CONFIG_DRM_FBDEV_EMULATION=y`, `tidss`), and no display was attached in any run, so only `/dev/dri/card0` exists. Source: `beagley-ai/bench/portability/os-survey.log`.

## Frigate

### Frigate's `nginx` exits with `bind() to 0.0.0.0:5000 failed (98: Address already in use)`

Move your own service off port 5000: Frigate's nginx takes it when the container uses host networking. The detection server in `beagley-ai/frigate/` listens on 5100. Source: `beagley-ai/frigate/README.md`.

### Frigate shows `detection_fps` 0.0 and the detector never gets a request

Frigate's motion detector is still calibrating, and it only sends regions to the detector when it is not. It leaves calibration when a frame has under 5 percent changed area and 4 or fewer motion boxes, so a busy or moving scene never does. Set `motion.threshold: 100` and `contour_area: 600` for such a clip (a fixed camera does not need it). Source: `beagley-ai/frigate/logs/tidl-c7x-clip.log`, `frigate/motion/improved_motion.py` at v0.18.2.

### Frigate logs `Error calling deepstack API: ... Read timed out`

The model takes longer than `api_timeout` (0.5 s in the sample config). The Arm-core model took 648 ms; set `api_timeout: 3.0`. Source: `beagley-ai/frigate/logs/onnx-a53-clip.log`.

### Frigate's camera input dies: `Error during demuxing: No such device`

On 2026-11-03 the webcam dropped off USB (`-71`, then `USB disconnect`) within seconds of Frigate's ffmpeg opening it at 1280x720 MJPEG, in 2 of 2 tries, on the board's earlier power setup; with `docker run --device` the container's device file then stays stale. On 2026-11-07, on a rated 5 V 3 A supply, Frigate read the same webcam for 130 s at 59.71 ms per detection, so the supply is the likely cause (the same as the webcam entry above, not isolated here). Start the container with `-v /dev:/dev` so it sees the camera. Source: `beagley-ai/frigate/logs/webcam-run.log`, `data/results/beagley-ai.csv` (`frigate-deepstack-yolox-nano-416-c7x-usb-webcam`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-08. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A model load on the C7x never returns and later model loads hang too; seen once (`beagley-ai/FAQ.md`, 2026-11-03)
- ffmpeg h264_v4l2m2m encode does not finish and ignores SIGTERM; kill -9 ends it and the codec still works (`beagley-ai/bench/video/ffmpeg-encode-hang-dmesg.log`, `beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`, `beagley-ai/FAQ.md`, 2026-11-03)
- gst-launch-1.0 prints 'Caught SIGSEGV' and waits instead of exiting when v4l2h264dec has capture-io-mode set or the videoconvert after it has n-threads; a pipeline that converts before it scales runs at 6.5 fps and looks hung (`beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`, `beagley-ai/bench/seam/v4l2-after-reboot.log`, `beagley-ai/FAQ.md`, 2026-11-03)
- The USB webcam drops off USB (-71, then disconnect) at 1080p or on a gimbal move, and returns about 5 s later with its aim reset; 7 drops in 35 min (`beagley-ai/bench/camera-usb-drops-2026-11-03.log`, 2026-11-03)
- Killing app_edgeai.py with pkill while it read an RTSP stream left it in uninterruptible sleep inside the Wave5 decoder release; dmesg showed 'wave5_vpu_firmware_command_queue_error_check: result not ready' and the next start failed with 'Error pulling tensor from GST Pipeline'; stopping with SIGINT and waiting 6 s did not hang (`beagley-ai/bench/camera-lan/results-2026-11-08.log`, 2026-11-08)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Reboot over ssh (`reboot`); the board answered ssh again about 45 s later (after a Wave5 decode failure the reboot cleared it; a hung C7x was cleared this way once and that hang was not logged; the Wave5 pipelines recover when the process is killed) | remote | proven | `beagley-ai/bench/seam/v4l2-after-reboot.log`, 2026-11-03 |

<!-- recovery:end -->
