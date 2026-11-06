# llama2.c on the LP-MSPM33C321A: a 260K-parameter story model in 544 kB of flash

A language model that writes children's stories, running on a Cortex-M33 with 1 MB of flash and 256 kB of RAM. The model is TinyStories 260K, the smallest checkpoint in the llama2.c family. The weights are quantized to int8 and live in internal flash. The key and value cache and all activations live in SRAM.

Terms are in [docs/vocab.md](../../docs/vocab.md).

Status: **Works** on the LP-MSPM33C321A. The image runs on the board at 21.2 tokens per second at 160 MHz (section "On the board"), matches the Mac token for token under greedy decoding, and writes its story live on the Educational BoosterPack screen. Under sampling the text drifts from the Mac's after token 39 (see Drift).

## What it uses

| | Flash | RAM |
|---|---|---|
| Image (`-O2`, TI Arm Clang 5.1.1.LTS) | 543.6 kB of 1,024 kB (53 %) | 192.6 kB of 256 kB (75 %), plus a 4 kB stack |
| Of which the model | 509.5 kB int8 weights and scales, 6.1 kB tokenizer | 160 kB key and value cache for 128 steps |
| Code | 27.6 kB | The 4 kB stack is enough: the deepest use seen in the emulator is 1.7 kB |

The float32 checkpoint is 1,031.8 kB. It alone is 7.8 kB more than the chip's flash, so the float model does not fit; int8 does, with room to spare.

## Numbers

| Check | Result |
|---|---|
| Port against the unmodified `runq.c` on the Mac, same int8 file | Byte-identical text on 15 of 15 runs (5 sampler settings, 3 prompts, 128 steps) |
| Int8 against float32 `run.c`, greedy, 128 steps | Identical text |
| M33 emulator against the Mac, greedy, 128 steps | All 128 token ids identical |
| M33 emulator against the Mac, sampled (temperature 1.0, top-p 0.9, seed 42) | 39 token ids identical, then the text goes its own way (see Drift) |
| Instructions per generated token on the M33 | 4,811,400 mean, 4,406,104 to 5,142,358, over 128 tokens (617.4 M instructions in all) |
| Tokens per second at 160 MHz (estimate made before the board run) | **Estimate: 26, within 20 to 33.** The board measured 21.4 (see below), inside the range. 160 MHz is 1.0 cycle per instruction at best (33) and, scaled by the CoreMark ratio between 32 MHz and 160 MHz on this chip (4.38 to 3.40 per MHz, 2 flash wait states), about 1.29 (26) |
| Mac reference, same prompt and seed | float32 `run.c`: about 2,900 to 3,100 tokens a second; int8 `runq.c` with 4-wide groups: about 1,200 to 1,600 (Apple silicon, `-O3`, one core) |

The emulator counts instructions, not cycles. It does not model flash wait states, the code cache or the FPU's latency.

The reference prompt is `Once upon a time`, temperature 1.0, top-p 0.9, seed 42, 128 steps. The Mac int8 run:

> Once upon a time, there was a big red ball. The ball was very tall and ordinary. The ball lived in a big garden. One day, a bird wanted to slip in the hill. The bird wanted to jump up a tree. The bird was very happy.
> The big bird slowly tried to open the ball, but the bird was too long. The bird f

The greedy run (temperature 0) is the stable comparison: it gives the same 128 token ids on the Mac, in the emulator and on the board.

## Drift

Greedy decoding picks the largest logit, so small rounding differences do not change it. Sampling draws against the whole probability list, so they do. At position 38 of the seed-42 run the logits on the Mac and on the M33 differ by up to 0.12 (1e-6 to 5e-6 at positions 0 and 4). The probabilities of the top token move from 0.558 to 0.544, the random draw lands in a different bucket, and the story continues differently. The cause is in the M33 build, not in the port: the math library calls (`expf`, `powf`, `sinf`, `cosf`) in TI's C library are not bit-identical to the Mac's, and each activation vector is re-quantized to int8, which turns a one-bit difference into a step of 1/127 of a group maximum. Turning off fused multiply-add changed nothing. Every host build of the port (`-O0`, `-O2`, with and without fused multiply-add) prints the same text.

## Why groups of four

The checkpoint's `hidden_dim` is 172, which is not a multiple of 64. `runq.c` quantizes in whole groups and rounds the vector length down, so with 64-wide groups it would silently drop the last 44 values of every feed-forward vector. The group size must divide both 64 and 172, which leaves 4. That doubles the file (one float scale for every four weights) and still fits. `tools/quantize_q80.py` refuses a group size that does not divide both.

## Ten-minute path

Needs the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 (the same set as `mspm33c321a/`), plus a Python with `numpy` (`pip install numpy`). Nothing in this block flashes a board.

```bash
cd ports/llama2c
PYTHON=python3 ./tools/check_host.sh      # Mac or Linux: port matches the unmodified runq.c (about 10 s)
PYTHON=python3 ./build.sh O2              # board image; first run downloads the model (1 MB) and makes the SysConfig files
PYTHON=python3 ./build.sh O2 emu          # same code for the emulator
python3 -m venv ~/llama2c-venv && ~/llama2c-venv/bin/pip install unicorn
~/llama2c-venv/bin/python -I host/emu_m33.py <emu build>/llama2c.out --nm <TI Arm Clang>/bin/tiarmnm --no-count
```

`--no-count` takes 3 s; counting instructions takes 11 minutes. Add `--stack-probe` for the stack depth. Build output goes to a folder the script prints (`OUT` changes it). Other options are listed at the top of `build.sh`: `PROMPT`, `TEMP`, `TOPP`, `SEED`, `STEPS`, `GS`.

### On the board

```bash
python3 ports/llama2c/run_board.py <builds>/llama2c-O2/llama2c.out
```

It flashes the image with DSLite, waits for the finished flag, reads the result block over the probe and prints the text, the token ids and tokens per second from the chip's cycle counter. The image touches no pin, UART or screen. The result lives in RAM (`ll_result`, `ll_out`, `ll_ids`, `ll_fwd_cycles`). With seed 42 and sampling, expect the same ids as the Mac for the first 39 tokens and a possible split after that.

## Files

- `src/llama2q.c`, `src/llama2q.h`: the port. Same arithmetic as `third-party/llama2c/runq.c`; no `malloc`, no files, no `stdio`. One row of the embedding table is dequantized per token instead of the whole table (saves 128 kB), the tokenizer copies its strings into a static pool, and the raw-byte token parse is done by hand instead of `sscanf`.
- `src/host_main.c`: Mac and Linux front end for the checks.
- `target-m33/main.c`: the board program. `build.sh`, `fetch.sh`, `run_board.py`, `host/emu_m33.py`, `tools/`.
- Upstream, unmodified: `third-party/llama2c/` (MIT, Copyright (c) 2023 Andrej Karpathy). The model is `karpathy/tinyllamas` stories260K, MIT per its model card; it is downloaded by `fetch.sh` into `model-cache/` (git-ignored) and its hash is checked.

## Limits

- 128 steps, prompt included. The cache costs 1.25 kB a step, so about 170 steps is the most that fits in RAM. The model itself allows 512.
- The prompt is at most 61 characters.
- Sized for this one checkpoint (`LL_DIM` and the other shape macros in `src/llama2q.h`); `ll_init` refuses any other.
- TinyStories text is short and repetitive. The model is the smallest one that makes sentences, not a chat model.

## On the board (LP-MSPM33C321A, 2026-10-21)

The int8 image (O2) ran on the LP-MSPM33C321A at 160 MHz, seed 42, 128 tokens:

| Measure | Result |
|---|---|
| Tokens per second, forward passes only | 21.4 |
| Tokens per second, whole generate call | 21.2 |
| Cycles per token | 7.47 M (6.82 to 8.05 M) |
| Token ids against the M33 emulator | 128 of 128 equal |
| Token ids against the Mac int8 build | first 39 equal, then the libm drift described above |

The story it wrote begins: "Once upon a time, there was a big red ball. The ball was very tall and ordinary."

`fetch.sh` checks both model files on every build and keeps a download only when its SHA-256 matches.

## On the screen (LP-MSPM33C321A with the Educational BoosterPack MKII, 2026-10-21)

The model writes its story live on the BoosterPack's 128 x 128 screen: each token's text is drawn as it is produced, words wrap at 21 columns, the text scrolls up after 15 lines, and a header shows tokens per second and the story number. A new story starts when S2 is pressed or 30 s after the last one began; the prompt and the seed change each time.

```bash
PYTHON=<python3 with numpy> EDU_CLK160=1 APP=llama2c bash portability/boosterpacks/edumkii/build.sh mspm33c321a
```

The program is `portability/boosterpacks/edumkii/examples/llama2c/llama2c.c`. Image: 567.3 kB of flash (54 %), 197.4 kB of RAM plus a 4 kB stack. The driver and its 5 x 7 font add 11 kB of flash and 0.2 kB of RAM to the plain image.

| Measure (128 tokens, 160 MHz, seed 1234) | Result |
|---|---|
| Tokens per second with the display on, whole generate call | 19.1 |
| The same without the drawing time | 21.1 |
| Cost of drawing | 9.9 % of the generate call (106 M of 1.07 G cycles) |
| Stories finished in the first 4 minutes | 9, one every 30 s |

The plain image measured 21.2 tokens per second, so drawing costs about 10 %. The screen path was judged from a camera: the text grows line by line, fills the screen and scrolls, and a new story begins on the 30 s cadence. The S2 press was not tested; its code path is a poll of the same pin the hello example reads. The board's results sit in the RAM struct `ls` (`stories`, `tokens`, `gen_cycles`, `disp_cycles`, `tps_x10`, `tps_nodisp_x10`, `scrolls`, `s2_presses`).
