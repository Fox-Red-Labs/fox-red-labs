<!-- experiment
id: E298
board: BeagleY-AI
date: 2026-11-03
status: works
reached: verified
line: Frigate 0.18.2 tracks two people and a dog with its detector on the C7x (64.46 ms per detection against 184.56 ms on its CPU detector); the USB webcam as the input drops off USB within seconds of Frigate opening it, 2 of 2 tries
stop: webcam input, on the camera's USB supply; a recorded clip stands in
-->
# Can Frigate use the BeagleY-AI's C7x accelerator as its object detector?

Terms: [docs/vocab.md](../vocab.md), BeagleY-AI FAQ. **Frigate** is the open-source home camera recorder behind many Home Assistant setups. 1 kB = 1,024 bytes.

**Short answer.** Yes, with a small server on the board between Frigate and the C7x. With the detector on the C7x Frigate reports 64.46 ms per detection and 6.6 detections a second without dropping a frame; its own CPU detector reports 184.56 ms, 4.5 a second and drops about 2 frames a second. The webcam did not work as the input, so the demo runs on a recorded clip.

**Question.** A home camera user wants local person detection without a Coral or a GPU. Frigate lists detectors for many accelerators and none for TI's. Does the BeagleY-AI's C7x work as one?

**Vendor position.** Frigate's documentation (docs.frigate.video/configuration/object_detectors, read 2026-11-03) lists Coral, Hailo, OpenVINO, ONNX on NVIDIA, AMD and Intel, Rockchip, Synaptics, Apple Silicon, MemryX, DeGirum and a DeepStack/CodeProject.AI detector that "is performed over the network". No TI detector. BeagleBoard's Edge AI page says the AM67A features are in its Edge AI image only.

**What we tried.** One BeagleY-AI 4 GB, Edge AI image 11.00.00.08, nothing else running on it. Frigate 0.18.2 (arm64 container) on the board itself, because the board has the C7x and its 2,529 MB was enough. The route was read from Frigate's source before building: the `deepstack` detector posts each region as a JPEG and reads a JSON list of boxes, and the `onnx` detector's runner file names no TIDL provider, so the ONNX path is closed. `beagley-ai/frigate/deepstack_tidl.py` implements the DeepStack call and runs the zoo's `yolox-nano-lite` 416x416 on onnxruntime's `TIDLExecutionProvider`. `beagley-ai/frigate/expectations.txt` was written before any run. Runs, each on the clip (TI's `video1_1280_768.h264`, two people and a dog, repeated 30 times) at 5 fps detect for 130 s:

1. Detector on the C7x through the server.
2. Frigate's CPU detector (3 threads), as the comparison.
3. The same model on the Arm cores through the same server, `api_timeout` 0.5 s, then 3 s.
4. The USB webcam at 1280x720 as Frigate's input, once with `--device` and once with `-v /dev:/dev`.

**How far it got.** Configure, compile, link, flash, boot, run: yes. Verified: Frigate's snapshot shows person 93 percent, person 84 percent and dog 83 percent (`beagley-ai/frigate/frigate-c7x-detections.jpg`).

| Number | Value | Row |
|---|---|---|
| Frigate inference speed, C7x | 64.46 ms | `dl_inference_ms`, `frigate-deepstack-yolox-nano-416-c7x` |
| Frigate inference speed, its CPU detector | 184.56 ms | `dl_inference_ms`, `frigate-cpu-tflite-ssdlite-300-a53` |
| Frigate inference speed, model on the Arm cores | 628.72 ms | `dl_inference_ms`, `frigate-deepstack-yolox-nano-416-a53` |
| Server per request, C7x (network 11.11 ms) | 20.03 ms | `dl_inference_ms`, `server-total-yolox-nano-416-c7x` |
| Detections a second, C7x / CPU detector / Arm cores | 6.6 / 4.5 / 1.5 | `gst_pipeline_fps`, `frigate-detection-fps-*` |
| Frigate container memory | 935.7 / 864.8 / 789.4 MiB | `linux_app_ram_mb` |

Expected against measured: the server's network time (10 to 13 ms, 11.11) and total (16 to 22 ms, 20.03) held, the CPU detector (80 to 200 ms, 184.56) held, memory held. Frigate's own figure missed (25 to 40 ms expected, 64.46 measured), and so did the Arm cores (450 to 520 ms, 629).

**The line.** Two things stopped or slowed it.

- The webcam. Frigate's ffmpeg opened the Insta360 Link 2 Pro at 1280x720 MJPEG and within seconds `dmesg` printed `Non-zero status (-71) in video completion handler` and `USB disconnect`; Frigate logged `Error during demuxing: No such device` and `Ffmpeg process crashed unexpectedly`. Both tries failed, and in the first the container's device file went stale when the camera came back. This is the drop already logged at 1080p (`beagley-ai/bench/camera-usb-drops-2026-11-03.log`), now also at 720p with a CPU-loaded board. The likely cause is the port's supply; not proven. The next try is a powered USB hub.
- Frigate ran no detection on the first clip at all. Its motion detector stays in "calibrating" until a frame has under 5 percent changed area and 4 or fewer motion boxes (`frigate/motion/improved_motion.py`), and detection regions are only added when it is not calibrating (`frigate/video/detect.py`). A busy, moving scene never leaves calibration, with either detector: the detector got no requests (`detection_fps` 0.0, the server's `n: 0`). Raising `motion.threshold` to 100 and `contour_area` to 600 fixed it for this clip. A fixed camera does not need this.

The 44 ms between the server's 20 ms and Frigate's 64 ms was not split further. The candidates are Frigate's JPEG encode, a new connection per request (the server answers HTTP/1.0) and the four cores shared with the clip decoder (about 80 percent of one core).

**Ideas we did not try.**

1. A powered USB hub for the webcam, then `config-camera.yml`; that gives the camera-in-the-loop number.
2. HTTP/1.1 keep-alive in the server, to see how much of the 44 ms is the connection.
3. Four camera streams on the one server: Frigate shares the detector between cameras and the server serialises the C7x, so the rate would divide by about four.
4. Frigate's MQTT output into Home Assistant (MQTT was off here), for the event latency.

**Limits.** One clip, one run per row; the person and dog scores come from one snapshot and are not an accuracy test. Frigate with one camera only, no recording, no snapshots, no MQTT, no Home Assistant. The CPU detector uses a smaller model (300x300 SSD) than the C7x run (416x416 YOLOX), so that comparison is Frigate's shipped detector against this board's, not one model on two chips; the same-model Arm-core run is the third row. The C7x run fed the network BGR, as the zoo's `param.yaml` asks, and the Arm-core run RGB; that changes scores slightly, not times. No temperature was logged.
