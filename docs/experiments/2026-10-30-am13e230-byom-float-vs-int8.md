<!-- experiment
id: E289
board: LP-AM13E230
date: 2026-10-30
status: works
reached: verified
line: Nine models in the files their training tools wrote (four float32 CNNs from torch, a Keras CNN as float32 and int8 TFLite, a decision tree and a random forest from scikit-learn, an XGBoost model) ran on the AM13E230 through a zero-dependency Python-to-C step with 20 of 20 classes matching the training tool on every float and tree model; float32 as trained runs at 0.8 to 1.2 times the time of the int8 plain-C build (0.392, 1.194, 5.581, 22.48 ms against 0.489, 1.25, 5.055, 18.71 ms at 200 MHz), the NPU build stays 2.4 to 8.9 times faster than float, the XGBoost model is the most accurate and runs in 80 us; the tool's own int8 quantization keeps 19 or 20 of 20 classes at 0.447 to 19.95 ms, with CMSIS-NN kernels 2.894 and 8.540 ms on the 8k and 30k models (the 30k faster than the vendor's NPU build), and its tuned float loops run 1.3 to 1.7 times faster than the plain ones. On 2026-10-31, standard ONNX Runtime int8 compiled for the NPU put 0 layers on it (CPU speed), TI's post-training option put every layer there with no retraining at a 0 to 9.7 point accuracy cost, and ResNet-8 through TI's QAT ran in 198.2 ms on the NPU build against 201.6 ms with CMSIS-NN, because the residual layers stay on the CPU.
stop: none
-->
# Does a model run on the LP-AM13E230 straight from the file the training tool wrote, in float, with no vendor quantization step?

**Short answer.** Yes, for all nine tried. A float32 1-D CNN trained by torch and exported to ONNX as is, a scikit-learn decision tree and random forest through skl2onnx, and an XGBoost model from its own JSON file each went to portable C with `tools/byom` (plain Python 3.10 or newer, no package installed) and gave the training tool's class on 20 of 20 held-out windows on the board, with outputs within 3e-6 of torch and 1.2e-7 of XGBoost. On the Cortex-M33's FPU at 200 MHz the float models take 0.392, 1.194, 5.581 and 22.48 ms (692 to 33.2k parameters), which is 0.80, 0.96, 1.10 and 1.20 times the int8 plain-C build of the same architectures. TI's NPU build of the int8 models stays 3.7, 5.8, 8.9 and 2.4 times faster than float. The trees take 1.4 us, 24 us and 80 us, and the XGBoost model has the best test-set accuracy of the seven (93.2 percent).

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. NPU is the chip's neural-network accelerator; NNC is TI's neural network compiler; ONNX is the exchange file format most training tools export; a multiply-add is one weight times one input.

**Question.** An engineer has a trained model in the form the training tool gives (an ONNX file, a scikit-learn or XGBoost model). Does it run on this chip as it is, how fast, and does it still give the same answers?

**Vendor position.** TI's NNC 2.1.2 user's guide takes an ONNX model quantized with TI's model optimizer (`tinyml_torchmodelopt`, 8-bit quantization-aware training) and compiles it for the NPU or the CPU (<https://software-dl.ti.com/mctools/nnc/mcu/users_guide/compiling.html>, read 2026-10-09). Its setup page asks for Python 3.10 and a Linux or Windows machine (read 2026-10-30); there is no macOS or Arm build, which is why this repo's NNC compiles ran on a GitHub Linux machine (E33). The guide's model types are neural networks; decision trees and gradient-boosted trees are not covered. The AM13E230 data sheet gives the Cortex-M33 a single-precision FPU.

**What was tried.** The seven models were trained on the Mac mini with the training tools' own exporters (`tools/byom/examples/train_occupancy.py`: torch 2.15 opset 17, scikit-learn 1.9 through skl2onnx 1.20, XGBoost 3.4 `save_model` JSON and onnxmltools 1.16), on the same UCI room-occupancy windows as the int8 models (`mspm0g3507/mcu-bench/edgeai/`): the four CNNs with the exact layer specs of `occ_cnn_500` to `occ_cnn_30k`, trained in float for 60 epochs and never quantized; the trees on 16 window features (last, mean, min and max of each channel). `tools/byom/byom.py check` read each file (its own protobuf reader, no `onnx` package), wrote portable C99 (float32 loops, two static buffers, no runtime), compiled it with the Mac's `cc` and compared it with the training tool's outputs on 20 held-out windows (5 per class), then wrote the vectors as a header. Then `am13e230/bench/byom/build.sh` built one image per model with TI Arm Clang 5.1.1.LTS at `-O3`, hard float, the AM13E2x SDK 26.01.00.03.STS start-up and the 200 MHz clock of TI's CoreMark example; `run.sh` erased, flashed and ran each image three times from reset and read the result block over the probe: the class of every vector against the training tool's class and the true label, the largest output difference, and the fastest of 5 calls per vector on the DWT cycle counter. Rows: `data/results/am13e230.csv` (`byom_latency`, `byom_agreement`).

**How far it got.** Verified: 24 of 24 runs gave 20 of 20 classes matching the training tool, and every run of an image gave the same cycle count on the fastest vector (the 30k model differed by 3 cycles on one run). Expectation, written before the runs (`_agent/runs/2026-10-30-byom/EXPECTATION.md`): float at 6 to 12 cycles per multiply-add, so 0.35 to 0.7, 1.0 to 2.0, 4.7 to 9.5 and 18 to 37 ms; all four landed inside (6.8, 7.0, 7.1 and 7.4 cycles per multiply-add). The tree guesses were low: under 1 us, 2 to 5 us and 10 to 30 us against 1.4, 24 and 80 us measured; a node costs about 35 to 40 cycles here (a 12-byte node read from flash with three wait states, a compare, a data-dependent branch and, at each leaf, one add per class), not the few cycles assumed.

| Model | Parameters | Multiply-adds | Test accuracy (float, this run) | float32 C from the ONNX file | int8 plain C `-O3` (E33 rows) | TI NNC CPU, int8 | TI NNC NPU, int8 | NPU gain over float |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| occ_cnn_500 | 692 | 11,456 | 88.8 % | 0.392 ms | 0.489 ms | 0.499 ms | 0.107 ms | 3.7x |
| occ_cnn_2k | 1,852 | 33,984 | 88.8 % | 1.194 ms | 1.25 ms | 1.376 ms | 0.205 ms | 5.8x |
| occ_cnn_8k | 8,684 | 157,952 | 92.9 % | 5.581 ms | 5.055 ms | 5.544 ms | 0.630 ms | 8.9x |
| occ_cnn_30k | 33.2k | 610,688 | 81.8 % | 22.48 ms | 18.71 ms | 22.91 ms | 9.535 ms | 2.4x |

The int8 columns are the 2026-10-08 and 2026-10-09 rows on the same board, clock and compiler; their models were trained from the same specs with TI's quantization-aware step, test accuracy 88.4, 88.5, 90.9 and 85.4 percent (`mspm0g3507/mcu-bench/edgeai/uci-room-occupancy/*/result.json`). The float and int8 models are separate training runs, so the accuracy columns compare recipes, not one model two ways. Parameters count the float model's as trained (including the input normalization); the int8 models carry 636 to 32,836 after folding.

| Model | Nodes | Weights in flash | Test accuracy | Fastest vector | Slowest vector | Image flash / RAM |
|---|--:|--:|--:|--:|--:|--:|
| scikit-learn decision tree, depth 8 | 65 | 1.8 kB | 87.8 % | 1.4 us | 2.2 us | 11.4 kB / 9.2 kB |
| scikit-learn random forest, 20 trees, depth 6 | 876 | 24.3 kB | 83.4 % | 24.3 us | 34.9 us | 32.3 kB / 9.2 kB |
| XGBoost, 120 trees, depth 4, from its JSON file | 1,290 | 22.4 kB | 93.2 % | 80.1 us | 113.3 us | 36.0 kB / 9.2 kB |
| the same XGBoost model through onnxmltools | 1,290 | 22.4 kB | 93.2 % | 80.2 us | 112.7 us | 36.0 kB / 9.2 kB |

Image sizes are the whole program (harness, 20 vectors, SDK start-up, two 1 kB to 4 kB work buffers on the CNNs); the model's own flash and working RAM are in each row's conditions. Test accuracy is on the 1,001-window test split of the block protocol; the 20 vectors are 5 per class, so "x of 20 match the true label" in the rows is a check of the vectors, not an accuracy figure (the 30k float model, the least accurate, matched 5 of 20).

## Second run, same day: TFLite, the tool's own int8, and tuned loops

Three more questions, run the same way (three runs each, expectation first in `_agent/runs/2026-10-30-byom/EXPECTATION-2.md`).

**A TFLite file.** The same CNN family built in Keras and exported with `tf.lite.TFLiteConverter`, float32 (6.1 kB) and int8 with a representative dataset (5.8 kB), read by the tool's own flatbuffer reader (`tools/byom/tflite.py`, no TensorFlow on the host). The float file gives the interpreter's class on 20 of 20 vectors within 1.9e-6 on the host and the board, and runs in 0.304 ms with the tuned loops. The int8 file is read on the float path (weights dequantized, the interpreter's activation requantization dropped): it agrees with the interpreter on 15 of 20 vectors. That file is also the first-person evidence for the quantization pain in the survey: TFLite's default int8 conversion took this model from 92.1 to 83.4 percent test accuracy, and its interpreter matches only 10 of 20 true labels on the vectors, because the per-tensor int8 constant that scales the four input channels (0.023 to 5.98) cannot hold the small values. The float file matches 18 of 20.

**The tool's own int8.** `byom.py quantize`: symmetric int8, one scale per output channel for weights, one per tensor for activations from the 20 vectors as the calibration set, int32 accumulation, exact integer requantization (the C and the Python reference give the same integers). Expectation: 6 to 9 cycles per multiply-add, so 0.34 to 0.52, 1.0 to 1.5, 4.7 to 7.1 and 18 to 27 ms; all four landed inside.

| Model | Int8 by the tool | Classes vs the float model | Largest output difference | Weights | TI's int8 path, CPU (NNC) | TI's NPU |
|---|--:|--:|--:|--:|--:|--:|
| occ_cnn_500 | 0.447 ms | 20 of 20 | 0.30 of 9.35 | 892 B | 0.499 ms | 0.107 ms |
| occ_cnn_2k | 1.227 ms | 19 of 20 | 0.26 of 6.06 | 2.3 kB | 1.376 ms | 0.205 ms |
| occ_cnn_8k | 5.386 ms | 20 of 20 | 0.44 of 8.8 | 10.1 kB | 5.544 ms | 0.630 ms |
| occ_cnn_30k | 19.95 ms | 20 of 20 | 0.17 of 8.31 | 35.5 kB | 22.91 ms | 9.535 ms |

So an int8 scheme any engineer can read (and any accelerator tool can be compared against) keeps the float model's answers on 79 of 80 vectors and runs within 1.2 times of the vendor's int8 CPU code. The NPU column is the vendor's and stays the only way under 1 ms on the 8k model.

**Tuned float loops (`--fast`).** A zero-padded copy of the input, no bounds checks in the inner loop, the output tile accumulated per weight. Expectation: 2 to 4 cycles per multiply-add. Measured 5.3, 5.0, 4.7 and 4.4 cycles per multiply-add: 0.302, 0.846, 3.703 and 13.57 ms, 1.3, 1.4, 1.5 and 1.7 times faster than the plain loop, short of the expectation. A second form that accumulates in a register instead of memory was measured slower on all four (0.344, 0.938, 3.928, 14.69 ms; rows `-fastreg`), so the first form is what `--fast` writes. The remaining gap to the NPU is 2.8, 4.1, 5.9 and 1.4 times. A real kernel library (CMSIS-NN, or the vendor's) is the next rung; these loops are what plain C gets to.

**A real kernel behind the same front end (`quantize --kernels cmsis-nn`).** The same int8 models, with the convolutions and the fully connected layer written as CMSIS-NN calls (`arm_convolve_s8`, `arm_fully_connected_s8`, Apache-2.0, built from `third-party/cmsis-nn` with the M33's DSP extension) and the pools kept as plain loops in CMSIS-NN's NHWC order. Expectation: 1.5 to 3 cycles per multiply-add. Measured 8.6, 3.7 and 2.8 cycles per multiply-add on the 500, 8k and 30k models: 0.491, 2.894 and 8.540 ms. The smallest model lost (0.447 ms plain): CMSIS-NN's per-call setup and im2col copy cost more than its 11k multiply-adds save. The 8k model gained 1.9 times over the plain int8 loop and the 30k model 2.3 times, and on the 30k model CMSIS-NN on the CPU (8.540 ms) is faster than the vendor's NPU build (9.535 ms), because the NPU build keeps one convolution on the CPU in plain code. Classes 20 of 20 against the float model on all three; the outputs matched the plain int8 reference to the count on every vector (CMSIS-NN's rounding did not differ here).

| Model | Plain int8 (ours) | CMSIS-NN int8 (ours) | Vendor int8, CPU | Vendor NPU |
|---|--:|--:|--:|--:|
| occ_cnn_500 | 0.447 ms | 0.491 ms | 0.499 ms | 0.107 ms |
| occ_cnn_8k | 5.386 ms | 2.894 ms | 5.544 ms | 0.630 ms |
| occ_cnn_30k | 19.95 ms | 8.540 ms | 22.91 ms | 9.535 ms |

**The vendor's compiler on an Arm host.** TI's index offers NNC 2.1.2 as a CPython 3.10 wheel for Linux x86-64 (and Windows); asked on 2026-10-30, it had nothing for Python 3.12 on any host and nothing for Linux aarch64 (`pip download` from an Apple Silicon Mac with Python 3.12, and from arm64 and amd64 Linux containers with Python 3.10 and 3.12). `tools/byom/nnc-container/` is a Docker image of that exact pair (python:3.10-slim, linux/amd64) with the wheel and TI Arm Clang 5.1.1.LTS inside, both checked against the sha256 values on record. On the Apple Silicon Mac mini, under Docker's x86 emulation, the image built in 2 min 2 s and compiled `occ_cnn_500` for the NPU in 10.0 s; its generated C differs from the GitHub-built copy (E33) only in two variable names and the input path. Linked and run here: 20 of 20 vectors, 0.1065 ms, against 0.1067 ms for the GitHub build (row `occ_cnn_500-nnc-npu-container`). So the wheel's host limits are a packaging fact, not a compiler one.

The tool also gained a page over the same commands (`tools/byom/web.py`: model and vectors in, table, check and a zip of the C out, on this computer only; with a target board chosen it builds, flashes and shows the measured time, checked on the AM13E230), a LICENSE (BSD-3 on the tool, nothing on the generated C) and a run on an Arm Mac mini with a stock Python 3.12 and no packages (both checks in 0.7 s).

**A second board, 2026-10-31: the LP-MSPM0G5187 (Cortex-M0+, no FPU, 80 MHz).** The same C for `occ_float_500` (tuned loops) and `xgb`, the same 20 vectors and the same three-run method, through `mspm0g5187/bench/byom/` (SysTick cycle counter, DSLite flash, OpenOCD read-back). Expectation written first: 11 to 23 ms for the CNN (80 to 160 cycles per multiply-add in software float) and 0.3 to 0.6 ms for XGBoost. Measured: 29.09 ms (2.33 million cycles, about 203 cycles per multiply-add) and 1.01 ms (80.8k cycles, about 170 cycles per node visited), 20 of 20 classes on both, every run within 4 cycles of the others. Both are above the expectation: the soft-float library costs more per operation than assumed. Against the AM13E230 (0.302 and 0.080 ms) the M0+ is 96 and 13 times slower; the clock explains 2.5 times of that, software float the rest, and trees suffer less because a tree is compares and branches. Rows: `byom_latency` and `byom_agreement` in `data/results/mspm0g5187.csv`.

**An image model, 2026-10-31: MLPerf Tiny ResNet-8 on CIFAR-10.** The benchmark's own float file (`pretrainedResnet.tflite` from mlcommons/tiny: 78k parameters, 12.5 million multiply-adds per 32x32 colour image), read by `tools/byom` after it gained skip connections (a tensor read twice is kept in a slot; a two-tensor ADD adds the slot back; the int8 add is exact integer arithmetic). Expectation first (`_agent/runs/2026-10-31-resnet/EXPECTATION.md`): float does not fit, int8 at 0.38 to 0.56 s plain and 0.18 to 0.23 s with CMSIS-NN.

| ResNet-8 on the LP-AM13E230, 200 MHz | Weights | Working RAM | Time per image | Matches float (20 images) | CIFAR-10 accuracy (1,000 images, host) |
|---|--:|--:|--:|--:|--:|
| float32 as trained | 304 kB | 224 kB | does not fit: the link needs 297 kB of RAM, the chip has 96 kB | 20 of 20 on the host | 88.1 % |
| int8 by the tool, plain C | 79 kB | 56 kB | 444.6 ms (7.1 cycles per multiply-add) | 20 of 20 | 88.4 % |
| int8 by the tool, CMSIS-NN | 79 kB | 56 kB | 201.6 ms (3.2 cycles per multiply-add) | 20 of 20 | same integers to within one count |

Both times landed inside the expectation; three runs each, cycle-identical to within 3 cycles. The TFLite interpreter scores 88.1 percent on the same 1,000 images and MLPerf's own int8 file 87.8, so the tool's int8, calibrated on 20 images, loses nothing. The 20 float test images take 245 kB of flash, so the board build lets constants span both flash banks (the linker's split operator on `.rodata`). The NPU build follows in the third-day section below. Rows: `byom_latency` and `byom_agreement`, variants `resnet8-*`, in `data/results/am13e230.csv`.

## Third day, 2026-10-31: does the NPU take a standard int8 model?

TI's NNC guide says the NPU needs a model "quantized with the TI-NPU quantization scheme" and that float or CPU-quantized models run their layers on the CPU (<https://software-dl.ti.com/mctools/nnc/mcu/users_guide/ti-npu-qat.html>, read 2026-10-31). This run measures that. The same four float CNNs from the first run (retrained with the same recipe, logits within 4e-6 of the shipped ONNX files) were quantized three ways and each compiled by NNC 2.1.2 with `--target="c, ti-npu"` in `tools/byom/nnc-container` on the Mac mini, then flashed and run three times each with `am13e230/bench/occupancy-nnc/` (expected class: the float model's). Expectation first (`_agent/runs/2026-10-31-npu-std-int8/EXPECTATION.md`).

- **ONNX Runtime int8** (`quantize_static`, QDQ format, per-channel weights, MinMax on 256 training windows), the way most engineers quantize an ONNX file. Two forms: everything quantized, and the input normalization left in float (the sensor channels span 0 to 1 for PIR and 365 to 1,075 ppm for CO2, so one int8 scale on the raw input crushes the small channels).
- **TI's post-training quantization** (`TINPUTinyMLPTQFxModule` in `tinyml_torchmodelopt` 1.5.0, the PTQ path of TI's own examples: calibration passes with no backpropagation, then convert and export). It installed with `--no-deps` on torch 2.15 on the Arm mini.

| Model | ORT int8, all quantized | ORT int8, input in float | TI PTQ | Float model, CPU (first run) | TI QAT, NPU (E33) |
|---|--:|--:|--:|--:|--:|
| NPU layers (NNC report) | 0 | 0 | all | n/a | all |
| occ_cnn_500 | 0.655 ms, 6 of 20 | 0.646 ms, 20 of 20 | 0.107 ms, 7 of 20 | 0.392 ms | 0.107 ms |
| occ_cnn_2k | 1.608 ms, 10 of 20 | 1.558 ms, 19 of 20 | 0.205 ms, 10 of 20 | 1.194 ms | 0.205 ms |
| occ_cnn_8k | 6.447 ms, 9 of 20 | 6.439 ms, 19 of 20 | 0.630 ms, 10 of 20 | 5.581 ms | 0.630 ms |
| occ_cnn_30k | 21.38 ms, 6 of 20 | 21.31 ms, 20 of 20 | 9.458 ms, 12 of 20 | 22.48 ms | 9.535 ms |
| Test accuracy (1,001 windows, host), float 88.8 / 88.8 / 92.9 / 81.8 % | 81.4 / 85.8 / 84.7 / 78.9 % | 89.0 / 88.7 / 92.9 / 81.7 % | 87.0 / 84.3 / 83.2 / 81.8 % | | |

"x of 20" is the board's class against the float model's on the 20 vectors; the host gives the same count on every build (onnxruntime on the same file). The TI QAT column is a separate training run (E33), shown for its time.

What it shows:

- **A standard int8 model compiles for the NPU target and runs entirely on the CPU.** NNC's offload report lists every operator of every ONNX Runtime build as not offloaded; it considered only the max-pool (refused: NCHW layout and input padding) and never matched a convolution. The builds run in NNC's generic C at 0.95 to 1.65 times the time of the float model on the M33's FPU (the smallest model loses most), so quantizing this way buys nothing in time on this chip.
- **TI's own post-training option reaches the NPU with no retraining.** Every layer offloads on three models (the 30k keeps one convolution on the CPU: NNC warns its weights and bias could overflow the 18-bit accumulator), and the NPU times land within 1 percent of the QAT builds' (0.1067, 0.2051, 0.6299 and 9.458 ms against 0.1067, 0.2051, 0.6297 and 9.535 ms). The price is accuracy: 0 to 9.7 points of test accuracy on these models, and 7 to 12 of 20 vectors agreeing with float, against 19 or 20 for ONNX Runtime with the input left in float. QAT exists to win that back.
- **The answer to "does TI need its own scheme?" is yes, measured**: the NPU's pattern matcher takes TI's quantized graph form (the Mul, Floor and Clip requantization TI's exporter writes) and nothing else. TI's PTQ is the cheapest way in.

Rows: `ai_occ_latency`, variants `occ_float_*-ort-int8-nnc`, `-ort-int8-infloat-nnc` and `-ti-ptq-nnc-npu`, in `data/results/am13e230.csv`; each row's note holds the NPU layer count and the agreement.

**ResNet-8 on the NPU, same day.** TI's NPU path needs a torch model through TI's quantization-aware training, so ResNet-8 was rebuilt in torch from the MLPerf weights (88.1 % on the first 1,000 test images, the same as the TFLite float file) and put through `TINPUTinyMLQATFxModule` (8-bit, 8 epochs on CIFAR-10 with crop and flip, about 75 s an epoch on the Mac mini's CPU). Two things broke on the way. Torch's FX lowering refused the graph where a fused add and ReLU also fed the explicit pad that copies TF's bottom-and-right "same" padding on the stride-2 convs; moving the ReLU after the pad got past that, but TI's convert step then produced a model that gave class 5 for every image (10 percent) although the fake-quantized model scored 88.6. The standard PyTorch block (padding 1 on every 3x3 conv, no pad modules) converts correctly; it shifts the stride-2 convs by one pixel against MLPerf's weights (float falls to 60.2 percent), so the float model was fine-tuned 6 epochs from those weights (87.2 percent on 10,000 images) before QAT. Result: the int8 ONNX file scores 86.3 percent on 10,000 test images and 87.1 on the first 1,000. Expectation first (`_agent/runs/2026-10-31-resnet-npu/EXPECTATION.md`, revised before any board run): 120 to 250 ms, about the CMSIS-NN time.

| ResNet-8 on the LP-AM13E230, 200 MHz | On the NPU (NNC report) | RAM | Time per image | Board vs host, 20 images |
|---|---|--:|--:|--:|
| int8 by tools/byom, CMSIS-NN, CPU (above) | nothing | 56 kB | 201.6 ms | 20 of 20 |
| TI QAT, NNC `ti-npu` | 4 of 9 convs, the average pool and the FC | 85.7 kB workspace | 198.2 ms | 20 of 20 (18 of 20 true labels) |
| TI QAT, NNC CPU only (`type=soft`) | nothing | 106.1 kB workspace | does not fit: 96 kB of data RAM | |

The NPU build is 1.02 times faster than our CMSIS-NN build: NNC leaves the five convs that feed a residual add (the second 3x3 conv of each block and the two 1x1 shortcuts, 7.3 of 12.5 million multiply-adds) and the three adds on the CPU, where they run as NNC's generic C. TI's NPU layer table has no standalone add and a 1x1 convolution with a residual input (PWCONVRES) that this graph did not match (<https://software-dl.ti.com/mctools/nnc/mcu/users_guide/layer_configs_npu.html>). The NPU build also only fits because its workspace is smaller than the CPU build's: the harness's 12 kB input buffer and result block had to move to the 32 kB code RAM (`am13e230/bench/resnet-nnc/linker.sh`). Three runs, 39.64 million cycles on all three. Rows: `ai_ic_latency` and `ai_ic_agreement`, variant `resnet8-ti-qat-nnc-*`.

**Published ResNet-8 results from other vendors, beside ours.** The image model above is MLPerf Tiny's image-classification task, so published results exist for the same model file and data set (<https://github.com/mlcommons/tiny/tree/master/benchmark/training/image_classification>). MLPerf Tiny v1.4 (closed division, July 2026, <https://github.com/mlcommons/tiny_results_v1.4>) has STMicroelectronics results on seven systems; time per image below is 1 / the median throughput in each system's `ic/performance/results.txt`, and the core, clock and stack come from its `systems/*.json`. Every ST entry used "TensorFlow Int8 Quantization" and STEdgeAI-Core. No NXP part appears in any MLPerf Tiny round, v0.5 to v1.4, closed or open (the submitter folders of all seven results repositories, read 2026-10-31), so there is no published NXP number on this task to put here.

| ResNet-8, one 32x32 image | Core, clock | Engine | Time | Million cycles | Top-1 | Source |
|---|---|---|--:|--:|--:|---|
| STM32H7P5 (system status: Preview) | Cortex-M7, 550 MHz | Neural-ART NPU, 550 MHz | 0.96 ms | 0.5 | 88 % | v1.4 `NUCLEO_H7P5II_NPU` |
| STM32H7P5 (Preview) | Cortex-M7, 550 MHz | CPU | 21.1 ms | 11.6 | 87 % | v1.4 `NUCLEO_H7P5II_CM7` |
| STM32H7A3 | Cortex-M7, 280 MHz | CPU | 41.7 ms | 11.7 | 87 % | v1.4 `NUCLEO_H7A3ZI_Q` |
| STM32U3C5 | Cortex-M33, 96 MHz | HSP accelerator | 50.0 ms | 4.8 | 87 % | v1.4 `NUCLEO_U3C5ZIT6_Q_HSP` |
| STM32L4R5 | Cortex-M4, 120 MHz | CPU | 165.6 ms | 19.9 | 87 % | v1.4 `NUCLEO_L4R5ZI-P` |
| STM32U3C5 | Cortex-M33, 96 MHz | CPU | 182.7 ms | 17.5 | 87 % | v1.4 `NUCLEO_U3C5ZIT6_Q_CM33` |
| AM13E230 (ours) | Cortex-M33, 200 MHz | CPU, tools/byom int8 + CMSIS-NN | 201.6 ms | 40.3 | 88.4 % (1,000 images) | rows `resnet8-*` |
| AM13E230 (ours) | Cortex-M33, 200 MHz | TI NPU build (TI QAT + NNC) | 198.2 ms | 39.6 | 87.1 % (1,000 images) | row `resnet8-ti-qat-nnc-npu` |

How to read it. MLPerf's top-1 is on the benchmark's own accuracy subset, ours on the first 1,000 test images, so the accuracy column is a sanity check, not a ranking. On the same core type, ST's CPU build of the Cortex-M33 takes 17.5 million cycles per image and ours 40.3 million: STEdgeAI's generated code does the image in 2.3 times fewer cycles than tools/byom's int8 plus CMSIS-NN, so our CPU column is not the M33's best. Clock and memory differ too: the AM13E230's code runs from flash with 3 wait states at 200 MHz. ST's NPU entry ran a model quantized with TensorFlow's standard int8 tool; whether the Neural-ART compiler needs anything beyond that for other models was not checked here.

**The line.** Nothing stopped. Three limits on what the numbers say:

- The C is a reference implementation (one plain loop nest per layer, bounds checks in the inner loop), so 7 cycles per multiply-add is a floor for "any C compiler, any core", not what a tuned kernel or CMSIS-NN would reach on this core. Int8 plain C from the same kind of loops sits at 6 to 9 cycles per multiply-add here, which is why float as trained lands within 20 percent of it on a core with an FPU.
- The NPU gain is real and needs TI's quantization scheme: the NPU build is the only column that moves the 8k model under 1 ms, and that build only exists through TI's model optimizer (QAT, or its post-training option) and NNC. A float model and a standard int8 model have no NPU path (third-day section). On a residual image model the NPU takes under half the work and gains 2 percent.
- The AM263P (Cortex-R5F, FPU, no NPU) is off the bench, so its float figure is not measured here; its existing rows run TI's own float TVM models at 1.7 to 2.9 ms (`data/results/am263p.csv`, `ai_vendor_example`).

**Next moves.** CMSIS-NN or the vendor's kernels behind the same `byom` front end, to see how far the M33 can close on the NPU past plain C. The int8 TFLite arithmetic itself (the interpreter's requantization) so int8 TFLite files match their interpreter. The same harness on the LP-MSPM0G5187 (no FPU: float in software) and on an R5F board, to put one table across M0+, M33 and R5F. Train the float and int8 models from one checkpoint so the accuracy columns compare one model two ways.

**Limits.** One room, one dataset, 20 vectors per model on the board. Time is the fastest of 20 vectors, best of 5 calls, a best case with no sensor read or feature extraction (the tree models' 16 features were computed on the Mac). Energy was not measured. No peak-performance claim: none of the C is a tuned kernel.
