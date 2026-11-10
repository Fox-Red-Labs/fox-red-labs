<!-- experiment
id: E271
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: line found
reached: run
line: Speech to text, a 0.5B language model and two TI demo models ran on the two A53 cores with no accelerator. whisper.cpp tiny.en takes 24.5 s for an 11 s clip (2.2x real time) and base.en 52 s (4.8x); Qwen2.5-0.5B Q4_0 generates 2.9 tokens/s and reads a prompt at 5.2 tokens/s in 477 MB; YAMNet takes 20 ms per 0.975 s window and the Silero en_v5 speech model 0.87 s per 3 s window.
stop: cpu
page: none
-->
# What can the AM62L's two A53 cores do for edge AI with no accelerator?

**Question.** What can the AM62L's two A53 cores do for edge AI with no accelerator?

**How far it got.** Small classifiers are cheap, speech recognition is slower than real time, and a 0.5B language model talks at about 3 tokens per second. All numbers are medians of 3 on one board, both cores at 1.25 GHz, 1 kB = 1,024 bytes. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**Expectation (written before the runs).** whisper.cpp tiny.en 0.4 to 0.9 x real time, base.en 1 to 2.5; Qwen2.5-0.5B 1 to 3 tokens/s generation (memory-bandwidth ceiling 4 to 6 at the lab's 1.6 to 2.4 GB/s); YAMNet 40 to 150 ms; Silero 1 to 4 s per window. Source: the AM62L CoreMark and STREAM rows.

| Test | Threads | Result | Expected | Hit? |
|---|---|---|---|---|
| whisper.cpp tiny.en, 11.0 s clip, total time | 2 / 1 | 24.5 s (2.23x) / 44.0 s (4.00x) | 0.4 to 0.9x | No, 3x slower |
| whisper.cpp base.en | 2 / 1 | 52.2 s (4.75x) / 96.4 s (8.76x) | 1 to 2.5x | No |
| Qwen2.5-0.5B Q4_0 generation | 2 / 1 | 2.89 / 1.68 tokens/s | 1 to 3 | Yes |
| Qwen2.5-0.5B Q4_0 prompt | 2 / 1 | 5.23 / 2.84 tokens/s | 5 to 20 | Low end |
| YAMNet (LiteRT, XNNPACK) per 0.975 s window | 2 / 1 | 20.2 / 30.4 ms | 40 to 150 | Faster |
| Silero en_v5 (ONNX Runtime) per 3 s window | 2 / 1 | 868 / 1,440 ms | 1,000 to 4,000 | Faster |

**The line.** Speech recognition is slower than real time on these cores. Why whisper missed: the encoder always processes a 30 s window, and it is most of the time: 15.7 s of the 24.5 s tiny.en run (29.2 s of 44.0 s on one thread). The encode time falls 1.86x from one thread to two, so both cores are used. The expectation took the 11 s clip as the cost; the cost is set by the 30 s window, so a clip longer than 30 s costs proportionally more and a shorter one costs the same. No second method was run to check the absolute rate.

**What was tried.** whisper.cpp and llama.cpp built on the board (GGML_NATIVE, NEON, no dot-product instructions on A53; builds took about 1 h and 2.5 h with two cores). Models: ggml-tiny.en and base.en, qwen2.5-0.5b-instruct-q4_0.gguf. TI's `yamnet_audio_classification.tflite` and `en_v5_static.onnx` come with the SDK image (`/usr/share/oob-demo-assets/models`). Inference through pip packages (see Added to the board). Other processes stopped during every run.

**Added to the board** (all on the USB-stick root file system; none on eMMC): pip `cmake`, `ninja`, `onnxruntime` 1.31.0, `ai-edge-litert` 2.3.0 (with flatbuffers, protobuf, ml_dtypes, tqdm, typing-extensions, backports.strenum); git clones of whisper.cpp and llama.cpp and the model files in `/root/bw` (about 1 GB). The PS5 driver build is on the SD card and PS5 driver page.

**Not run.** The webserver demo's NNStreamer pipeline (the two models were run directly instead). Power during inference (no rail reading taken).

Rows: `data/results/am62l-evm.csv`, tests `linux_asr_rtf`, `linux_llm_tok_s`, `linux_infer_ms`.
