# MLPerf Tiny keyword spotting on the TinyEngine NPU of a TI LP-MSPM0G5187

The benchmark's DS-CNN keyword spotter, quantized with TI's post-training tool and compiled by NNC, runs in 18.70 ms (53.46 inferences a second) on the NPU at 80 MHz and gets 91.0 percent of the 4,890 test clips right (89.7 percent on the first 1,000, 0.3 points under the closed-division bar; two runs of TI's quantization-aware training scored lower, 88.9 on the board, see E297). This is MLPerf-Tiny-style, not an official submission. The CPU build does not link and ResNet-8 does not fit the 32 kB of RAM.

Terms: [vocab](../../docs/vocab.md). Board facts: [AGENTS.md of the board](../AGENTS.md). Experiment page: [E297](../../docs/experiments/2026-11-03-mspm0g5187-mlperf-tiny-kws.md). Checked 2026-11-03 on one LP-MSPM0G5187 (probe <probe serial>), MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, NNC 2.1.2, SysConfig 1.28.0. Upstream benchmark: `third-party/mlperf-tiny/` (mlcommons/tiny at 4addd0f, Apache-2.0).

## Files

| File | What it does |
|---|---|
| `host/kws_torch.py` | Rebuilds the reference model in PyTorch from `kws_ref_model_float32.tflite` (int8 weights, scale per channel) |
| `results/` | Both QAT int8 ONNX files, training logs, and the board run of the first (`swd_qat1_kws4890.json`) |
| `host/kws_tinpu.py` | `ptq` or `qat` (about 11.5 min an epoch on the Mac mini's CPU; `KWS_LR`, `KWS_AUG=1`): TI quantization, ONNX export, fold, score with onnxruntime. Runs on a machine with PyTorch, TensorFlow and `tinyml_torchmodelopt` |
| `host/make_kws01.py` | Writes the benchmark's bin files and `y_labels.csv` from the test split |
| `firmware/th_port.c`, `firmware/build.sh`, `firmware/mlperf80.syscfg` | The device side of the benchmark API on the backchannel UART; `-DSWD_HARNESS` builds the debugger-driven image used for the numbers |
| `host/swd_run.py` | Drives the SWD image through OpenOCD: clip in, logits and cycles out, core never halted |
| `host/run_runner.py` | Runs the unmodified benchmark runner against one named serial port (not run yet) |

## Run

1. Quantize: `python3 kws_tinpu.py ptq out 4000` (host), then compile `out/kws_ptq_int8.onnx` with `tools/byom/nnc-container` target `npu` and `cortex-m0plus`, adding `output_int=true` to the target string.
2. Build: `mspm0g5187/mlperf-tiny/firmware/build.sh <NNC output> kws_npu_int_swd -DSWD_HARNESS` from the repository root.
3. Flash with DSLite (`-e -f -v -u`), then `python3 host/swd_run.py <dut.out> <kws01 or kws4890 folder> <probe serial> <result.json> [clips]`.

## Numbers

18.704 ms per inference, 1.466 to 1.503 million cycles per call; 4,450 of 4,890 clips right; image 67.6 kB flash, 26.8 kB RAM. QAT build: same 18.70 ms, 4,432 of 4,890 right, 889 of the first 1,000. Rows `kws_latency`, `kws_accuracy`, `kws_fit` and `ic_fit` in `data/results/mspm0g5187.csv`.
