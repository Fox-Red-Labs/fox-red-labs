<!-- experiment
id: E297
board: LP-MSPM0G5187
date: 2026-11-03
status: works
reached: verified
line: The MLPerf Tiny keyword-spotting DS-CNN, quantized with TI's post-training tool and compiled by NNC for the TinyEngine NPU, runs in 18.70 ms (53.46 inferences a second) on the LP-MSPM0G5187 at 80 MHz with all 11 compute layers on the NPU and gets 91.0 percent of the 4,890 test clips right (89.7 percent on the first 1,000, under the closed-division bar of 90); the CPU build does not link and ResNet-8 does not fit the 32 kB of RAM.
stop: none
-->
# How fast is the LP-MSPM0G5187's NPU on MLPerf Tiny keyword spotting?

**Short answer.** 18.70 ms per inference (53.463 inferences a second, five windows of 70 within 0.01 percent) at 80 MHz, and 91.0 percent top-1 on the full 4,890-clip test split. This is MLPerf-Tiny-style, not an official submission: it uses the benchmark's model, data, input format and 70-inference timing loop, but the benchmark's UART runner has not been run on this board, and on the first 1,000 clips (the benchmark's accuracy-set size) it scores 89.7 percent, 0.3 points under the closed-division bar of 90.

Terms: [vocab](../vocab.md). NPU is the chip's neural processing unit; NNC is TI's neural network compiler; PTQ is post-training quantization; DS-CNN is the depthwise-separable convolution network the benchmark uses for keyword spotting; MFCC is the 49 by 10 audio feature the model reads.

**Question.** No TI part appears in any MLPerf Tiny round (v1.3 and v1.4 results folders, read 2026-11-03). What would the LP-MSPM0G5187 score on the benchmark's keyword-spotting and image-classification tasks, on the NPU and on the CPU?

**What was done.** Expectation first (`_agent/runs/2026-11-03-mlperf-tiny/EXPECTATION.md`): DS-CNN on the NPU 25 to 200 ms, central 60; CPU 0.7 to 1.1 s; ResNet-8 does not fit. The reference model is `kws_ref_model_float32.tflite` from mlcommons/tiny (commit 4addd0f, vendored in `third-party/mlperf-tiny/`), which stores int8 weights with a scale per channel; `host/kws_torch.py` rebuilds it in PyTorch (91.86 percent on the 4,890 test clips). TI's post-training tool (`TINPUTinyMLPTQFxModule`, 4,000 calibration clips from the training split) quantized it to 91.35 percent with no retraining. NNC 2.1.2 (`tools/byom/nnc-container`, Mac mini) offloads every compute layer: 1 full convolution, 5 depthwise-type (the four depthwise convolutions and the average pool), 4 pointwise, 1 fully connected; the pad, scale, round and clip at the head stay on the CPU. The image is 67.6 kB of flash and 26.8 kB of RAM with a 3 kB stack. The 4,890 clips were written as the benchmark's `make_bin_files.py` does (MFCC divided by the reference int8 model's input scale 0.5847 plus zero point 83); the board converts each byte back to float before the call.

**Method.** Two images share `firmware/th_port.c`. The UART image implements the benchmark's device API (`name`, `db`, `infer N W`, `results`, `timestamp`) so the unmodified host runner can drive it; it is built (`firmware/build.sh`) but has not run, because the backchannel UART pins PA10 and PA11 carry a loop wire on this bench (the board read back its own start-up text, `m-inad`). The measured numbers come from the second image, `-DSWD_HARNESS`: `host/swd_run.py` writes each clip into RAM through OpenOCD's memory port while the core runs, sets a flag, waits for it to clear and reads the 12 int8 logits and the SysTick cycle count. The core is never halted, which keeps clear of the NPU wedge in `mspm0g5187/AGENTS.md`. Timing follows the benchmark's performance script: 10 warm-ups, 70 timed inferences, 5 windows, median. The unit boundary is `tvmgen_default_run` with the input already in float RAM, as in the benchmark's reference ports (the byte-to-float step runs once per clip, outside the timed loop).

| Task | Where | Result | Source |
|---|---|---|---|
| Keyword spotting, 1 inference | NPU, 80 MHz | 18.704 ms, 1.49 million cycles per call (min 1.466, max 1.503) | `kws_latency` row |
| Keyword spotting, accuracy | NPU, 4,890 clips | 4,450 right, 91.00 percent (host onnxruntime, same int8 file: 91.35; float torch: 91.86) | `kws_accuracy` rows |
| Keyword spotting, accuracy | NPU, first 1,000 clips | 897 right, 89.7 percent, the same 897 as onnxruntime on the host | `kws_accuracy` rows |
| Keyword spotting | CPU, NNC build | does not link: 43.7 kB of working data against 32 kB of RAM | `kws_fit` row |
| Image classification, ResNet-8 | NPU or CPU | does not fit: NPU build 134.9 kB read-only data and 86.5 kB RAM; CPU build 81.0 kB and 106.1 kB | `ic_fit` row |

The written expectation for the NPU (25 to 200 ms) held. The first run of the harness gave 3 of 20 clips right: NNC's `skip_normalize` could not drop the input scaling because the model's pad comes first, so the model takes float input and the int8 buffer was read as floats. Feeding floats gave 18 of 20, the same as the host. The accuracy gap to the benchmark's bar is a quantization cost (float 91.86, int8 91.35 on the full set; the 1,000-clip subset is harder), and TI's quantization-aware training, which gave back 6 to 9 points on other models in E289, was tried the same day (section below) and did not help.

**Beside published MLPerf Tiny v1.4 closed-division keyword-spotting results** (median throughput from each `kws/performance/results.txt` in <https://github.com/mlcommons/tiny_results_v1.4>, read 2026-11-03; time is 1 over throughput; accuracy from `kws/accuracy`):

| System | Core, clock | Engine | Time | Top-1 |
|---|---|---|--:|--:|
| STM32H7A3 (ST) | Cortex-M7, 280 MHz | CPU | 11.5 ms | 90.2 % |
| STM32U3C5 (ST) | Cortex-M33, 96 MHz | HSP accelerator | 16.5 ms | 90.2 % |
| **LP-MSPM0G5187 (ours, not submitted)** | Cortex-M0+, 80 MHz | TinyEngine NPU | **18.7 ms** | 91.0 % (4,890), 89.7 % (1,000) |
| STM32L4R5 (ST) | Cortex-M4, 120 MHz | CPU | 43.4 ms | 90.2 % |
| STM32U3C5 (ST) | Cortex-M33, 96 MHz | CPU | 48.4 ms | 90.2 % |
| STM32U385 (ST) | Cortex-M33, 96 MHz | CPU | 48.4 ms | 90.2 % |

The same table's ResNet-8 rows (STM32 CPU 21 to 183 ms, AM13E230 198 ms) are in [E289](2026-10-30-am13e230-byom-float-vs-int8.md); the G5187 has no ResNet-8 number because the model does not fit. Compared with ST's Cortex-M33 CPU build the G5187's NPU is 2.6 times faster at a lower clock, and about 14 percent slower than ST's accelerator build. Energy was not measured.

**Limits.** One board, one run of each image. The data is regenerated from the benchmark's own scripts and ordering, not the EEMBC-distributed copy. Image classification, visual wake words and anomaly detection were not run; the CPU path has no latency. The official runner on the UART image is the open item (the loop wire on J4 pins 33 and 34 comes off, the J101 shunts go on). Files: `mspm0g5187/mlperf-tiny/`.

## Quantization-aware training, same day (does not clear the bar)

**Question.** Does TI's quantization-aware training (QAT, `TINPUTinyMLQATFxModule`, 8-bit weights and activations) bring the first 1,000 clips from 89.7 to 90 or more, with the reference DS-CNN unchanged? Expectation first (`_agent/runs/2026-11-03-mlperf-tiny/EXPECTATION.md`): 90.3 percent on the 1,000 (89.5 to 91.2), 91.5 on the full split, about 60 percent chance of clearing 90. Bounded at two training attempts.

**Method.** `host/kws_tinpu.py qat` on the Mac mini's CPU (torch 2.15, 85,511 training clips, about 11.5 minutes an epoch; a 10-epoch run was stopped after one epoch for time and is not counted). Attempt 1: 3 epochs, Adam 1e-4, cosine, no augmentation. Attempt 2: 3 epochs, Adam 3e-5, random time shift of up to 2 frames and noise at 3 percent of the feature spread, the epoch kept chosen on the validation split, never the test split. Each trained model was exported, folded and scored with onnxruntime on the host; attempt 1 was then compiled by NNC (same offload report as PTQ: all 11 compute layers on the NPU, same 56.9 kB image) and run on the board by the same SWD image and clips as the PTQ run.

| Model | Host onnxruntime, 1,000 clips | Host, 4,890 | Board, 1,000 | Board, 4,890 | Latency |
|---|--:|--:|--:|--:|--:|
| Float (torch) | | 91.86 % | | | |
| TI PTQ, 4,000 calibration clips | 89.8 % | 91.35 % | 89.7 % | 91.00 % | 18.70 ms |
| TI QAT, attempt 1 | 89.6 % | 90.59 % | 88.9 % | 90.63 % | 18.70 ms |
| TI QAT, attempt 2 | 89.0 % | 90.43 % | not flashed | not flashed | same graph |

**Result.** QAT did not help and does not clear the bar: both attempts score below PTQ on the host, and the board run of attempt 1 gets 88.9 percent on the first 1,000 (889 clips) and 90.63 on the full split (4,432). The written expectation missed by 1.4 points on the 1,000. Latency is the same 18.70 ms because the compiled graph is the same. Two things stand out. The fake-quantized model during training scores 89.9 to 90.5 on the 1,000 and 91.2 on the full split, so the exported int8 file loses 0.6 to 0.7 points in TI's convert-and-fold step; the training itself gave nothing over PTQ. And the reference model was trained with augmentation on raw audio, while the stored features here have none, so the training loss drops to 0.003 to 0.05 and the weights drift away from the reference without gaining on unseen clips. The board and the host differ by 7 clips on the 1,000 and 2 on the full split for the same file, as they did for PTQ.

**What is left.** The 1,000-clip subset is harder than the full split for every model here (float torch is 92 on the full set), so the PTQ number at 89.7 is within the spread of the host-board difference. What could still move it, none tried under the two-attempt limit: a QAT run with the benchmark's audio-level augmentation (features regenerated each epoch), training from the benchmark's own training script rather than from the exported tflite weights, or a calibration set chosen to match the 1,000. Files: `mspm0g5187/mlperf-tiny/results/` (both int8 ONNX files, training logs, the board run).
