# BeagleY-AI (TI AM67A): notes for an AI coding assistant

That the board has an accelerator that runs a model 15 to 100 times faster than its Arm cores (two threads), a video codec that works through GStreamer, when frames are scaled before the CPU touches them, but not reliably through FFmpeg, and no way to reach it except Ethernet. This file is the short list; each item comes from a run on one board on 2026-11-03.

Errors and fixes: the [BeagleY-AI FAQ](FAQ.md). Terms: [docs/vocab.md](../docs/vocab.md). **C7x/MMA** is the DSP core and matrix accelerator; **TIDL** is TI's runtime that offloads a network to it; **Wave5** is the H.264 and H.265 codec block; **model zoo** is the precompiled networks in `/opt/model_zoo`. 1 kB = 1,024 bytes. Checked 2026-11-03 with BeagleBoard Edge AI image 11.00.00.08 (Linux 6.12.33), GCC 13.3.0, onnxruntime 1.15.0.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives.

## The board

| Fact | Value | Source |
|---|---|---|
| Chip | AM67A (J722S): 4 x Cortex-A53 at 1.4 GHz without ARMv8.1 atomics, 2 x C7x with MMA, R5F cores, Wave5 codec | `beagley-ai/FIRST-BOOT.log`; `beagley-ai/bench/portability/lse-trap-run.log` |
| Memory | 4 GB LPDDR4, 2,529 MB visible to Linux; no swap | `beagley-ai/FIRST-BOOT.log` |
| Access | Ethernet and ssh as root, no password; the USB-C port is power only | `beagley-ai/FIRST-BOOT.log` |
| Tools on the image | GCC 13.3.0, CMake, git, Docker, Python 3.12.9, onnxruntime 1.15.0 with `TIDLExecutionProvider`, GStreamer, FFmpeg; no `libc.a`, no KVM, no framebuffer | `beagley-ai/bench/portability/three-ways-run.log`, `beagley-ai/FIRST-BOOT.log` |
| Not on the board | an ADC (no `/sys/bus/iio` device), a fan | `beagley-ai/bench/portability/three-ways-run.log` |

## Start here

| Step | Command or file |
|---|---|
| Write the card | on a Mac, with a 32 GB or larger card (not in the box): `diskutil list external physical`, then `beagley-ai/tools/write-card.sh diskN beagley-ai-edge-ai-11.00.00.08-2025-09-06.img.xz` (refuses the Mac's system disk; the link and SHA-256 are in `docs/arrivals/beagley-ai.md`) |
| Power and network | Ethernet first, then a 5 V 3 A USB-C supply (not in the box); the USB-C port is power only (`beagley-ai/FIRST-BOOT.log`) |
| Log in | `ssh root@<board address>` (address from the router; host name `j722s-evm`) |
| Time one model on the C7x | `ONLY='ONR-KD-7060*' timeout 300 python3 /root/zoo_bench.py 30` after copying `beagley-ai/bench/zoo/zoo_bench.py` |
| Same on the Arm cores | add `cpu` as the second argument |
| Build a C program | `gcc` on the board; no cross compiler (`beagley-ai/bench/portability/three-ways-run.log`) |
| Choose inference and video backends at run time | `beagley-ai/bench/seam/glue.py` (set `FRL_INFER` to `tidl` or `cpu`, `FRL_VIDEO` to `v4l2` or `sw`) |

## Rules, each from a run

- Run one model per process, with `timeout`. A model load hung the C7x once until a reboot (`beagley-ai/bench/zoo/zoo_bench.py` takes `ONLY`; no log of the hang).
- Expect 43.13 ms for the pose model on the C7x and 4,475.57 ms on the Arm cores (`data/results/beagley-ai.csv`, `dl_inference_ms`, `yolox-s-pose-640-*`).
- Offload check: the ORT log for the pose model shows 1 subgraph, 274 of 274 nodes on the C7x (`beagley-ai/bench/zoo/zoo_tidl.log` row note in `data/results/beagley-ai.csv`).
- Decode with GStreamer, not FFmpeg, when frames go to a program: FFmpeg's `h264_v4l2m2m` delivered 3 frames then waited (`beagley-ai/bench/seam/ffmpeg-vs-gst-2026-11-03.log`).
- Encode with GStreamer `v4l2h264enc` (110.1 fps), never `ffmpeg -c:v h264_v4l2m2m` (`beagley-ai/bench/video/enc.log`, `beagley-ai/bench/video/ffmpeg-encode-hang-dmesg.log`).
- Hardware decode into a program: scale before converting (`v4l2h264dec ! videoscale n-threads=4 ! 224x224 ! videoconvert ! RGB`), 76.5 fps for 1280x768 to 224x224 RGB, 5 of 5 runs; converting first gives 6.5 fps (`beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`).
- Never set `capture-io-mode` on `v4l2h264dec` or `n-threads` on the `videoconvert` after it: `Caught SIGSEGV`, then `gst-launch-1.0` waits instead of exiting (same log).
- `result not ready: 0x800` in `dmesg` prints once per decode, also on runs that finish; it is not the failure (same log).
- Chain decode into encode with `v4l2h264enc output-io-mode=dmabuf-import`: 135.1 fps at 1920x1088 against 35.1 fps without it (`beagley-ai/bench/video/wave5-pipe-fix-2026-11-03.log`).
- A program with ARMv8.1 atomics dies with `Illegal instruction`; preload `lse_trap.so` from `am62l-evm/steam/lse_trap/` (builds here, 2 tests PASS: `beagley-ai/bench/portability/lse-trap-run.log`).
- The Linux glue for the portable app builds unchanged; only five config lines name this board's files (`beagley-ai/bench/portability/three-ways-run.log`, `portability/matrix.csv` X56).
- Pi-hole needs `FTLCONF_dns_listeningMode=BIND` because `systemd-resolved` holds port 53 (`beagley-ai/bench/docker/pihole-attempt1-LOCAL-mode-failed.log`).
- Home Assistant container: 52.7 s to the first HTTP answer, 531 MB (`beagley-ai/bench/docker/ha-and-pull.log`).
- Pop the bubbles (`beagley-ai/games/pop-the-bubbles/`): pose on the C7x 13 a second, 59 ms in the game; the game draws 20 fps at 1280x720 and 23 in the browser at 960x540 after caching the background (it rebuilt a gradient and a mirrored camera resize, 101 ms, every frame); JPEG encoding limits the browser stream (`beagley-ai/games/pop-the-bubbles/speedup-2026-11-04.log`).
- LLM and speech on the Arm cores are slow: 6.87 tokens/s for Qwen2.5 0.5B, speech real-time factor 0.66 to 0.90 for tiny.en (`beagley-ai/bench/llm/llama-bench.log`, `beagley-ai/bench/whisper/whisper-bench.log`). Neither uses the C7x.
- No display has been attached (`beagley-ai/FIRST-BOOT.log`: no `/dev/fb*`). A USB webcam (Insta360 Link 2 Pro) is at `/dev/video-usb-cam0` after `beagley-ai/tools/camera/setup.sh`; on a board fed from a USB hub port it dropped off USB at 1080p, after gimbal moves, and even at 720p or on a microphone recording once the board was busy; power the board from a rated 5 V 3 A supply before trusting any camera run; on one, the camera on the board's own USB hub held five 60 s starts at 720p, so a powered hub is the fallback if it still drops (`beagley-ai/bench/camera-usb-drops-2026-11-03.log`, `beagley-ai/bench/camera-power/e294-five-starts-2026-11-07.log`).
- The board dropped off Ethernet and rebooted 4 of 5 times within about 15 s of starting TI's camera app (and once in 2 on the clip) on a wall supply of unread rating (`beagley-ai/bench/camera-power/probe-2026-11-05.log`). On a rated 5 V 3 A supply 5 of 5 starts held 60 s at 28.0 to 29.4 fps and the board stayed up (`beagley-ai/bench/camera-power/e294-five-starts-2026-11-07.log`). Cause suspected to be the supply, not measured; stream your ssh output to the Mac, the board keeps no log.
- The camera app's on-screen people count read 0 in all of those runs because the camera faced the bench; no person has been counted (`beagley-ai/bench/camera-power/camera-frame-e294-start1-2026-11-07.jpg`).
- A stale `node server.js` keeps port 8081 after a start; `pkill -f server.js` (not `remote_streaming/server.js`) before `board_start.sh`.
- TI's `app_edgeai.py` needs `SOC=j722s` in its environment (a login shell sets it; plain `ssh host cmd` does not), or it runs the model on the Arm cores at 1.3 fps instead of 30.0 (`beagley-ai/bench/camdetect/clip-run-2026-11-04.log`).
- TI's `app_edgeai.py` under `nohup` needs `-n` (no curses report), and its detection post-processing crashed with `IndexError: index 5 is out of bounds` on a frame with exactly one detection; `beagley-ai/bench/camdetect/people_count.patch` carries the fix (`board_start.sh` applies it).
- Frigate 0.18.2 runs on the board with the C7x as its detector through the `deepstack` detector and `beagley-ai/frigate/deepstack_tidl.py`: 64.46 ms per detection against 184.56 ms on Frigate's CPU detector (`beagley-ai/frigate/logs/tidl-c7x-clip.log`, `beagley-ai/frigate/logs/cpu-tflite-clip.log`). Frigate's `onnx` detector names no TIDL provider.
- Run the detection server on port 5100: with host networking Frigate's nginx owns 5000 and exits with `bind() to 0.0.0.0:5000 failed (98: Address already in use)` (`beagley-ai/frigate/README.md`, 2026-11-03).
- Raise Frigate's `api_timeout` above the model time: at 0.5 s the Arm-core model (648 ms) gave 5 `Read timed out` errors and no detections (`beagley-ai/frigate/logs/server-stats.log`).
- A busy or moving scene keeps Frigate's motion detector calibrating and detection never runs (`detection_fps` 0.0); `motion.threshold: 100` with `contour_area: 600` fixed it for the sample clip (`beagley-ai/frigate/logs/tidl-c7x-clip.log`).
- Frigate's ffmpeg opening the webcam at 1280x720 dropped it off USB in 2 of 2 tries (`beagley-ai/frigate/logs/webcam-run.log`); a container started with `--device` keeps a stale device file after the camera returns.

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
