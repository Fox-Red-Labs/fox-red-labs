# Can Frigate use the BeagleY-AI's C7x accelerator as its object detector?

Yes, through Frigate's built-in `deepstack` detector and a 100-line server on the board: Frigate 0.18.2 tracked two people and a dog on a looped clip with the detector on the C7x, at 64.46 ms per detection (the same measure was 184.56 ms on Frigate's own CPU detector and 628.72 ms with the model on the four Arm cores). The USB webcam as the input did not hold: it dropped off USB within seconds of Frigate opening it.

Terms: [docs/vocab.md](../../docs/vocab.md), [BeagleY-AI FAQ](../FAQ.md). **Frigate** is the open-source home camera recorder that Home Assistant users run; **C7x** is the board's DSP core; **TIDL** is TI's runtime that offloads a network to it. 1 kB = 1,024 bytes; MiB as Docker prints them.

## The setup

| Part | What it is | Where it came from |
|---|---|---|
| `deepstack_tidl.py` | Server on port 5100: takes Frigate's JPEG, runs the zoo's `yolox-nano-lite` (COCO, 416x416) through onnxruntime's `TIDLExecutionProvider`, answers in DeepStack's JSON | this folder |
| Frigate 0.18.2 | `ghcr.io/blakeblackshear/frigate:0.18.2-standard-arm64`, 4.52 GB, pulled in 9 min | Frigate release of 2026-10-15 |
| `config-clip.yml` | Frigate config with the `deepstack` detector and the clip as input | this folder |
| `config-clip-cpu.yml` | Same, with Frigate's own CPU detector (ssdlite mobilenet v2 300x300) | this folder |
| `config-camera.yml` | The clip config with the USB webcam at 1280x720 as input | this folder |
| `sample.sh`, `probe.py` | Ten-second sampling of Frigate's stats; a client that posts JPEGs the way Frigate does | this folder |

Frigate's own `onnx` detector cannot reach the C7x: `frigate/detectors/detection_runners.py` at v0.18.2 names no TIDL provider. The `deepstack` detector (`frigate/detectors/plugins/deepstack.py`) posts `image` and `api_key` as a multipart form to `api_url` and reads `predictions` with `label`, `confidence`, `x_min`, `y_min`, `x_max`, `y_max`; it stops at the first prediction under 0.4 or with a label not in its label map.

## Ten-minute path

On the BeagleY-AI (BeagleBoard Edge AI image, as root), with Docker running:

1. Copy this folder to `/root/frigate/` and pull Frigate (9 minutes):
   ```
   docker pull ghcr.io/blakeblackshear/frigate:0.18.2-standard-arm64
   ```
2. Get Frigate's label list, so the server only returns labels Frigate knows:
   ```
   docker run --rm --entrypoint cat ghcr.io/blakeblackshear/frigate:0.18.2-standard-arm64 /labelmap.txt > labelmap.txt
   ```
3. Start the server (about 15 s to load the network onto the C7x) and check it:
   ```
   FRL_LABELS=/root/frigate/labelmap.txt setsid nohup timeout 3000 python3 deepstack_tidl.py > server.log 2>&1 < /dev/null &
   python3 probe.py http://localhost:5100/v1/vision/detection 10 /opt/edgeai-test-data/images/0000.jpg
   ```
   You should see `elephant` at 0.944 and an `inferenceMs` near 11.
4. Make a clip with people in it (TI's `video1_1280_768.h264`, repeated because a raw H.264 file does not loop), put the config in place and start Frigate:
   ```
   mkdir -p cfg media; for i in $(seq 30); do cat /opt/edgeai-test-data/videos/video1_1280_768.h264; done > media/yard30.h264
   cp config-clip.yml cfg/config.yml
   docker run -d --name frigate --network host --shm-size 256m -v /root/frigate/cfg:/config -v /root/frigate/media:/media ghcr.io/blakeblackshear/frigate:0.18.2-standard-arm64
   ```
5. After about 75 s, `sh sample.sh tidl-c7x-clip 130 yard` prints Frigate's inference speed and detection rate every 10 s and saves `out/tidl-c7x-clip.jpg` with the boxes (`frigate-c7x-detections.jpg` here is that picture).

## What was measured

All on one BeagleY-AI 4 GB, no fan, one test at a time, Frigate at 5 fps detect. Numbers are medians of the 10-second samples after the first, one run each.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Frigate inference speed, detector on the C7x | 64.46 ms | `deepstack` detector, yolox-nano 416 | measured | `logs/tidl-c7x-clip.log` |
| Same, Frigate's CPU detector | 184.56 ms | ssdlite mobilenet v2 300, 3 threads | measured | `logs/cpu-tflite-clip.log` |
| Same, the model on the Arm cores through the server | 628.72 ms | `api_timeout` 3 s | measured | `logs/onnx-a53-clip-t3.log` |
| Time inside the server per request, C7x | 20.03 ms | decode 5.46, preprocess 2.54, network 11.11, post 0.91 | measured | `logs/server-stats.log` |
| Detector calls per second, C7x / CPU detector / Arm cores | 6.6 / 4.5 / 1.5 | frames skipped: 0 / 2.0 / 3.8 per s | measured | the three `logs/*clip*.log` |
| Frigate container memory | 935.7 / 864.8 / 789.4 MiB | C7x / CPU detector / Arm cores; 2,529 MB visible | measured | the same logs |
| Person and dog found | 93, 84 and 83 percent | snapshot after 130 s | measured | `frigate-c7x-detections.jpg` |
| USB webcam 1280x720 as input | failed, 2 of 2 | dropped off USB within seconds | measured | `logs/webcam-run.log` |

Expected before the runs (`expectations.txt`) against what happened: server network 10 to 13 ms (11.11, held); server total 16 to 22 ms (20.03, held); Frigate's figure 25 to 40 ms (64.46, missed: about 44 ms sit between the server and Frigate, in its JPEG encode, the HTTP call and a board whose cores also decode the clip); Arm cores 450 to 520 ms (567 in the server, 629 in Frigate, missed: Frigate takes cores from the model); CPU detector 80 to 200 ms (184.56, held); memory 700 to 1,200 MiB (held).

## If you see this

- `bind() to 0.0.0.0:5000 failed (98: Address already in use)` in Frigate's log: Frigate's nginx owns port 5000 on host networking; run the server on 5100.
- `Error calling deepstack API: ... Read timed out`: the model is slower than `api_timeout` (0.5 s in the config). Raise it to 3 for the Arm-core run.
- `detection_fps` stays `0.0` and the server's `/stats` shows `n: 0`: Frigate's motion detector is still calibrating. Set `motion.threshold: 100` and `contour_area: 600` for a busy clip.

More: [BeagleY-AI FAQ](../FAQ.md).

## Where these facts come from

BeagleBoard Edge AI image 11.00.00.08, Linux 6.12.33, Docker 25.0.3, onnxruntime 1.15.0 with TIDL, Frigate 0.18.2 (source read at tag v0.18.2: `frigate/video/detect.py`, `frigate/motion/improved_motion.py`, `frigate/detectors/plugins/deepstack.py`, `frigate/detectors/detection_runners.py`), checked 2026-11-03. Rows: `data/results/beagley-ai.csv`, variants starting `frigate-` and `server-`. Experiment page: `docs/experiments/2026-11-03-beagley-ai-frigate-c7x.md`. Assistant notes: `beagley-ai/AGENTS.md`.
