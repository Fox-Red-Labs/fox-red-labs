<!-- experiment
id: E150
board: SK-TDA4VM
date: 2026-10-09
status: works
reached: verified
line: NVIDIA's Holoscan SDK cannot run on the TDA4VM (its build and runtime need CUDA; the board has no NVIDIA GPU and the R5F cores run TI's RTOS firmware with no Linux). The same operator graph as Holoscan's body-pose app, rebuilt from TI parts, runs end to end on the board: YOLOX-s pose at 640x640 takes 13.8 ms per inference on the C7x/MMA accelerator and 1,767 ms on the two Cortex-A72 cores alone (128 times slower, fp32 against int8); the whole graph from H.264 decode to an encoded output file runs at 17.8 fps on TI's short left-1280x768 clip (30 fps, paced by the clip, on a longer clip later), with 176 MB of process memory for the accelerated run.
stop: none
-->

# Can NVIDIA's Holoscan body-pose demo run on the SK-TDA4VM, and what does the same graph cost on TI's parts?

Terms used here are in [docs/vocab.md](../vocab.md). Board page: [tda4/README.md](../../tda4/README.md).

**Question.** Note (2026-10-10): this page answers a narrower reading of the question. The actual target is the Holoscan Sensor Bridge role (camera into the TI chip, Hololink frames out over Ethernet), which this page does not cover. The numbers here stand as TDA4VM vision measurements.

Can the NVIDIA Holoscan body-pose estimation app, or its benchmark, run on the SK-TDA4VM, on the Arm cores or the R5F cores? If not, what does the same graph cost in compute and memory when it is built from TI's own parts?

**Vendor position.** NVIDIA, Holoscan SDK install guide (read 2026-10-09): the SDK packages need the CUDA runtime "even for CPU only pipelines"; the CMake build enables the CUDA language unconditionally; operators sit on NPP, TensorRT, Vulkan and NVIDIA's GXF runtime. License Apache-2.0.

TI, Edge AI SDK getting started and the model zoo (11.00.00.08): the SK-TDA4VM runs deep-learning models on the C7x DSP with its matrix accelerator (MMA) through TIDL, called from ONNX Runtime, TFLite or GStreamer's `tidlinferer`. TI's human-pose model `ONR-KD-7060-human-pose-yolox-s-640x640` is precompiled on the image. TI publishes no figure for this model on this board; its data-sheet page gives 2.03 ms for MobileNetV1 (224), 3.04 ms for regNetx-200mf, 5.03 ms for yolox-nano-lite (416) and 5.9 ms for ssdLite mobDet (320).

**What was tried.** Board: SK-TDA4VM (J721EXSKG01EVM, PROC112A1), TI Edge AI SDK 11.00.00.08 (Linux 6.12.17-ti, Arago 2025.01, Python 3.12, onnxruntime 1.15.0 with the TIDL execution provider, GStreamer with the `tiovx` and `tidl` plugins). No display, no Ethernet; everything over the serial console.

1. **Is Holoscan there, or installable?** Looked for `/usr/local/cuda`, `libcudart`, `nvcc` and `import holoscan` on the board. Not read: the Holoscan wheel's dependency metadata (would need a download; not done).
2. **The same graph with TI parts.** Holoscan's body-pose app is source, format convert, inference, post-process, visualize. TI's `edgeai-gst-apps` runs that chain as a GStreamer pipeline: `filesrc` to hardware H.264 decode, `tiovxmultiscaler` and `tiovxdlpreproc` (hardware scaler and OpenVX kernels), `tidlinferer` on the C7x/MMA, Python post-process on the A72, hardware H.264 encode to an MKV file (no display attached). Config: `tda4/tools/pose_file.yaml`; TI's sample clip `left-1280x768.h264`.
3. **Inference alone, both ways.** `tda4/tools/ort_bench.py` and `zoo_bench.py`: the same ONNX model through ONNX Runtime with the TIDL provider (C7x/MMA, int8 artifacts from the zoo) and with the CPU provider (two A72 threads, fp32 graph as shipped). Random input, one warm-up, median of 30 or 50 runs (3 on the CPU). Then every other model in `/opt/model_zoo` the same way.
4. **R5F.** Checked what the six Cortex-R5F cores run: `/sys/class/remoteproc` shows the main-domain pairs attached to TI's RTOS firmware (IPC, vision-apps services); they have no Linux, no Python, and no TIDL runtime. Nothing was run on them.

**How far it got.** Measured. Every step ran; no step was blocked.

| Item | Result |
|---|---|
| CUDA on the board | none: no `/usr/local/cuda`, no `libcudart`, no `nvcc`; `import holoscan` fails. Holoscan cannot be built or installed here |
| Pose model, C7x/MMA (TIDL) | 13.8 ms per inference (median of 30; 13.9 over 50), all 274 graph nodes on the accelerator, 176 MB process RSS, 2.5 s to load |
| Pose model, two A72 cores (ONNX Runtime CPU, fp32) | 1,767 ms per inference (median of 3; 1,786 over 5), 154 MB RSS |
| Ratio | 128x. The CPU run is fp32 and the accelerator run is int8, so this is the cost of falling back to the CPU, not a like-for-like core comparison |
| Whole graph, file in to encoded file out | 17.8 to 18.1 fps, 55 to 56 ms per frame by the app's own metric, two runs. Hardware decode and encode, accelerator inference, Python post-process |
| Memory the board gives Linux | 2,170 MB of the 4 GB; the rest is carved out (639 MB vision-apps heap for the DSPs and R5Fs, 512 MB CMA, 32 MB R5F network buffers; `dmesg`) |
| R5F | not a target: RTOS firmware, no Linux, no TIDL. The accelerator for this work is the C7x; the A72 pair can run the fp32 graph at 0.6 fps |

The rest of TI's zoo, inference only, same method (`data/results/tda4.csv`, test `dl_inference_ms`):

| Model | Input | C7x/MMA ms | A72 ms | Ratio |
|---|---|---|---|---|
| MobileNetV1 mlperf (TFLite) | 224 | 2.17 | not run | |
| regNetx-200mf | 224 | 2.89 | 37.4 | 13x |
| yolox-nano-lite | 416 | 4.66 | 193 | 41x |
| ssd-lite mobv2 | 512 | 6.10 | 439 | 72x |
| ssdLite mobDet (TFLite) | 320 | 6.40 | not run | |
| deeplabv3lite mobv2 ade20k | 512 | 7.30 | 714 | 98x |
| yolox-s-lite widerface | 640 | 11.16 | 1,504 | 135x |
| yolox-s-lite coco | 640 | 11.68 | 1,523 | 130x |
| deeplabv3lite mobv2 robokit | 768x432 | 11.75 | 901 | 77x |
| deeplabv3 mobv2 ade20k mlperf (TFLite) | 512 | 12.11 | not run | |
| yolox-s pose | 640 | 13.76 | 1,767 | 128x |

TI's four published figures are within 7 to 10 percent of the values measured here for the same models (MobileNetV1 2.03 against 2.17, regNetx 3.04 against 2.89, yolox-nano 5.03 against 4.66, ssdLite 5.9 against 6.40); the values measured here include Python call overhead and a random input.

**The line.** Holoscan: no CUDA, so none of it runs here; that is a hard dependency, not a port effort. The graph it describes runs on the TDA4VM at 17.8 fps from decode to encoded output on TI's short left-1280x768 clip (a later run on a longer clip held 30 fps, paced by the clip), limited by the serial Python post-process and frame pull (56 ms per frame) rather than by inference (13.8 ms). On the A72 cores alone the same model gives 0.57 fps.

- The C++ `app_edgeai` or an all-GStreamer pipeline with `tidlpostproc`, which should lift the graph from 18 fps toward the 30 fps clip rate.

- A quantized (int8) ONNX graph for the CPU run, for a fairer core-to-core number.
- The two TFLite models and the three TVM models on the CPU (TVM needs the DLR runtime; not scripted).
- The small graph shim (`opgraph`) so the app code reads like a Holoscan app; and the second graph, tool tracking, after its licenses are read.
- Camera input and a display, once a webcam and a monitor are on the bench.

**Ask.** A reader who has run Holoscan's body-pose benchmark on an NVIDIA developer kit: what frame rate and memory did you see, with which model and input size, so the two graphs can be set side by side with the chips named.
