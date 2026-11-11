# How do you bring up the BeagleY-AI (TI AM67A) from a Mac and run a vision model on its C7x accelerator?

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

Write the BeagleBoard Edge AI image to a microSD card, plug in Ethernet and USB-C power, log in over ssh as root with no password, and the pose model from the model zoo runs in 43.1 ms on the C7x accelerator (the same model takes 4.48 s on the Cortex-A53 cores alone, with two threads). No display, camera or serial adapter is needed for that. Errors and fixes: [FAQ](FAQ.md). Notes for an AI assistant: [AGENTS.md](AGENTS.md). What happened, in order: TIMELINE.md.

Terms: [docs/vocab.md](../docs/vocab.md) and the [FAQ](FAQ.md). 1 kB = 1,024 bytes. **C7x/MMA** is the AM67A's DSP core and its matrix accelerator, the part that runs neural networks. **TIDL** is TI's deep learning runtime that offloads a network to the C7x. **Wave5** is the video codec block (H.264 and H.265 decode and encode). **Edge AI image** is BeagleBoard's build of TI's Edge AI Linux release. **Model zoo** is the folder of precompiled networks at `/opt/model_zoo` on that image.

Checked 2026-11-03 on one BeagleY-AI (4 GB) with BeagleBoard Edge AI 11.00.00.08 (built 2025-09-06), Linux 6.12.33, GCC 13.3.0, onnxruntime 1.15.0.

## The board

| Part | Fact | Read from |
|---|---|---|
| Chip | AM67A (J722S): four Cortex-A53 at 1.4 GHz, two C7x DSP cores with MMA, R5F cores, Wave5 codec, GPU | `data/boards.csv`; `beagley-ai/FIRST-BOOT.log` (`lscpu`, `/sys/class/remoteproc`) |
| Memory | 4 GB LPDDR4; Linux sees 2,529 MB, the rest is reserved for the accelerators and firmware | `beagley-ai/FIRST-BOOT.log` (`free -m`) |
| Storage | microSD only; a 128 GB card gave a 119 GB root partition | `beagley-ai/FIRST-BOOT.log` (`lsblk`) |
| Power | USB-C, 5 V at 3 A; the USB-C port carries power only on this image | [FAQ](FAQ.md) |
| Network | Gigabit Ethernet (used for every run), Wi-Fi 6 and Bluetooth LE (not used) | `beagley-ai/FIRST-BOOT.log` (`ip addr`) |
| Debugger | none needed; the JTAG pads are not used | `docs/arrivals/beagley-ai.md` |
| LEDs, buttons | LEDs `ACT` and `PWR` under `/sys/class/leds` | `beagley-ai/bench/portability/three-ways-run.log` |
| Video | micro-HDMI out (not connected in any run); `/dev/video0` is the Wave5 decoder, `/dev/video1` the encoder | `beagley-ai/FIRST-BOOT.log` |
| Camera | a USB webcam (Insta360 Link 2 Pro) at `/dev/video-usb-cam0`; stable at 1280x720, drops off USB at 1080p on the board's port | `beagley-ai/bench/camera-usb-drops-2026-11-03.log` |
| Not on the board | an ADC (no `/sys/bus/iio` device), a fan (a 4-pin fan connector only), a framebuffer without a display | `beagley-ai/bench/portability/three-ways-run.log` |

## Ten-minute path

Steps 1 to 5 were run on 2026-11-03; their total time was not recorded.

1. Mac, terminal. Download `beagley-ai-edge-ai-11.00.00.08-2025-09-06.img.xz` from BeagleBoard (`docs/arrivals/beagley-ai.md` has the link and the SHA-256) and write it to a 32 GB or larger card. Find the card's number first:

   ```bash
   diskutil list external physical
   beagley-ai/tools/write-card.sh disk4 beagley-ai-edge-ai-11.00.00.08-2025-09-06.img.xz
   ```

   You should see the SHA-256 check pass, then about 10.6 GB written. The script's refusals were run against the Mac's system disk and a 2 TB USB drive; a full card write with its current version has not been run (see the FAQ). If the Mac refuses the card, see the [FAQ](FAQ.md).
2. Bench. Put the card in the board, plug in Ethernet, then the USB-C power cable (a 5 V 3 A supply, C to C cable).
3. Router or terminal. Find the board's address in your router's client list; the board's name is `j722s-evm`. Log in:

   ```bash
   ssh root@<board address>
   ```

   You should get a shell with no password prompt.
4. Board shell. Check that the accelerator and the models are there:

   ```bash
   ls /opt/model_zoo
   python3 -c "import onnxruntime as o; print(o.get_available_providers())"
   ```

   You should see 14 model folders and `TIDLExecutionProvider` in the list.
5. Mac, terminal. Copy the benchmark script to the board and run the pose model, one model per process:

   ```bash
   scp beagley-ai/bench/zoo/zoo_bench.py root@<board address>:/root/
   ssh root@<board address> "ONLY='ONR-KD-7060*' timeout 300 python3 /root/zoo_bench.py 30"
   ```

   You should see one line ending in `median 43.1 ms` (43.13 on 2026-11-03). Run `ONLY` with other folder names for the other models; add `cpu` as a second argument to run the ONNX models on the Cortex-A53 cores.

## One-command build

The board has its own GCC, CMake, git and Docker, so a program builds on the board with no cross compiler. The portable app from [`portability/three-ways`](../portability/three-ways/README.md) builds with:

```bash
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Icommon -Ilinux -DCONFIG_AM62L_H -include linux/boards/beagley-ai/config_beagley_ai.h common/app.c linux/glue_linux.c -o three_ways
```

Built in 2.3 s; the stripped program is 66.1 kB. It does not build a static program (the image has no `libc.a`) and does not read an ADC (there is none).

## What was measured

Every number is a row in [`data/results/beagley-ai.csv`](../data/results/beagley-ai.csv). One board, one session; the model rows are the median of 30 runs (3 on the Cortex-A53 cores).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Pose model (YOLOX-S, 640x640) on the C7x | 43.13 ms | all 274 nodes offloaded; no fan | measured | `dl_inference_ms`, `yolox-s-pose-640-onnxrt-tidl-c7x` |
| The same pose model on the Cortex-A53 cores | 4,475.57 ms | onnxruntime CPU provider, 2 intra-op threads | measured | `dl_inference_ms`, `yolox-s-pose-640-onnxrt-a53` |
| Classifier (RegNetX-200MF, 224x224) on the C7x | 5.95 ms | same method | measured | `dl_inference_ms`, `regnetx-200mf-224-onnxrt-tidl-c7x` |
| Object detector (YOLOX-nano, 416x416) on the C7x | 10.15 ms | same method | measured | `dl_inference_ms`, `yolox-nano-lite-416-onnxrt-tidl-c7x` |
| H.264 decode, 1080p, hardware | 255.9 fps | GStreamer `v4l2h264dec` to `fakesink`, 27 to 29 percent of the four cores' time | measured | `video_decode_fps`, `1920x1088-h264-1` |
| H.264 decode, 1080p, software | 47.1 fps | GStreamer `avdec_h264`, four threads | measured | `video_sw_decode_fps`, `h264-1920x1088-gst-avdec-t4` |
| H.264 encode, 1080p, hardware | 110.1 fps | raw NV12 file in | measured | `video_encode_fps`, `1088p-h264-rawfile` |
| Decode and encode chained, 1080p | 135.1 fps | encoder imports the decoder's buffers (`output-io-mode=dmabuf-import`); 35.1 fps without | measured | `video_encode_fps`, `1088p-h264-transcode-dmabuf-import` |
| Hardware decode into a program, 1280x768 to 224x224 RGB | 76.5 fps | scale before converting; 6.5 fps the other way round | measured | `video_decode_fps`, `1280x768-h264-v4l2dec-scale224-rgb-pipe` |
| Decode plus classifier on the C7x, whole app | 61.3 fps | `bench/seam/app.py`, hardware decode | measured | `gst_pipeline_fps`, `gst-v4l2dec-scalefirst-regnetx-tidl-python-pipe` |
| llama.cpp v0.8.0, Qwen2.5 0.5B Q4_K_M, generation | 6.87 tokens/s | four threads | measured | `linux_llm_tok_s`, `qwen2.5-0.5b-q4_k_m-gen-t4` |
| whisper.cpp v1.9.5, tiny.en | real-time factor 0.66 to 0.90 | 54 s and 11 s clips, four threads | measured | `linux_asr_rtf`, `tiny.en-t4-speech54s` |
| Home Assistant container, first answer | 52.7 s | container 531 MB | measured | `linux_app_ready_s`, `ha-first-http-answer` |
| Portable app, 14 lines | gaps 0.94 to 1.00 s | glue file byte-identical to the AM62L's | measured | `portability/matrix.csv` X56 |

Real-time factor above 1 means slower than real time, for example 1.32 for base.en on the 54 s clip.

## If you see this

- If `ssh` is refused or the board does not show on the network, plug the USB-C cable into a power supply, not a computer: on the Edge AI image the USB-C port is power only and does not appear as a network adapter.
- If a model load never returns and later models hang too, reboot the board; run one model per process with `timeout`. See the [FAQ](FAQ.md).
- If `ffmpeg -c:v h264_v4l2m2m` encode never finishes and `dmesg` prints `wave5_vpu_firmware_command_queue_error_check: result not ready: 0x800`, use GStreamer (`v4l2h264enc`) for hardware encode instead.

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

BeagleY-AI 4 GB (sticker serial BYAI 202509 001054), BeagleBoard Edge AI image 11.00.00.08 built 2025-09-06 (Arago 2025.01, Linux 6.12.33), GCC 13.3.0, Python 3.12.9, onnxruntime 1.15.0 with the TIDL execution provider, llama.cpp v0.8.0, whisper.cpp v1.9.5, Docker; runs of 2026-11-03 from an Apple Mac over Ethernet. Data: [`data/results/beagley-ai.csv`](../data/results/beagley-ai.csv), `FIRST-BOOT.log`, the logs under `bench/`, `TIMELINE.md`, and this folder's [`AGENTS.md`](AGENTS.md) and [`FAQ.md`](FAQ.md). The arrival plan is `docs/arrivals/beagley-ai.md`. Family guide: [`portability/families/am67a/README.md`](../portability/families/am67a/README.md).

## What else is in this folder

| Folder | What |
|---|---|
| `bench/zoo/` | model-zoo timings on the C7x and the Cortex-A53 cores, with the script |
| `bench/video/`, `bench/llm/`, `bench/whisper/`, `bench/docker/` | the video, language model, speech and container runs, with their scripts and logs |
| `bench/portability/` | the portable app and the AM62L's atomics shim, built and run on this board |
| `bench/seam/` | one small app that picks its inference and video backend at run time |
| `bench/assistant/` | a private chat page and Mac dictation on the board: first word 1.4 s, 11 s of speech in 3.8 s |
| `bench/camdetect/` | E294: object detection with a people count, streamed to a browser page; 30.0 fps on a recorded clip; with the USB camera 28.0 to 29.4 fps, 5 of 5 starts held 60 s on a rated 5 V 3 A supply; on the earlier wall supply (rating unread) 0 of 5 held 15 s and the board rebooted (cause suspected to be the supply, not measured; no person counted yet, count 0; `bench/camera-power/`) |
| `games/pop-the-bubbles/` | P63: a body-pose game for small children; played on the board from a recorded clip and from the USB camera (20 s, 29.8 draw fps, pose 11.0 a second, wall supply; on a rated 5 V 3 A supply the browser stream held 60 s at 24.0 draw fps median, pose 10.6 a second) |
| `tools/write-card.sh` | write the Edge AI image to a card on a Mac, SHA-256 checked |
| `tools/camera/` | `setup.sh <board address>`: names a USB webcam `/dev/video-usb-cam0` on every plug-in and sets 60 Hz |
