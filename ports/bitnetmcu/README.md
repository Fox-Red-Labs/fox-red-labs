# Can a TI LP-MSPM0G3507 run BitNetMCU's handwriting-recognition neural net, and how does it compare with the WCH chip it was written for?

Yes. All four BitNetMCU models read all 10 of its test digits right on the LP-MSPM0G3507, the same answers as on a Mac, and at 32 MHz each took fewer clock cycles than the author's published count on WCH's CH32V002: 2,640,383 against 2,736,250 for the most accurate model (99.55% on MNIST), 3.5% fewer. At 80 MHz, with two flash wait states like his chip, it took 0.5% more. At 80 MHz that model answers in 34 ms, and a touchscreen demo lets you draw a digit with a finger and see the answer.

Terms: [docs/vocab.md](../../docs/vocab.md) (CNN, MNIST, flash wait state, SysTick, stack). 1 kB = 1,024 bytes. Measured 2026-10-06 on an Intel Mac, macOS 15.7.9: TI Arm Clang 5.1.1.LTS -O3, MSPM0 SDK 2.11.00.07, xPack OpenOCD 0.12.0+dev, one LP-MSPM0G3507 (probe <probe serial>). BitNetMCU commit `0715bfc` ([`third-party/bitnetmcu/UPSTREAM.txt`](../../third-party/bitnetmcu/UPSTREAM.txt)), copied unchanged.

**BitNetMCU** (github.com/cpldcpu/BitNetMCU, 344 stars, GPL-3.0) is an open project (GitHub user cpldcpu) for neural nets small enough for 10-cent microcontrollers: it reads 16 x 16 pixel handwritten digits. Its author publishes cycles per answer on a CH32V002 (RISC-V, 48 MHz, 4 kB RAM), which makes it a fair like-for-like test.

## Expected and measured

Written before the first board run: the board gives the same 10 answers as the Mac, and a cycle count close to the author's, because both are small 32-bit cores with a hardware multiplier.

| Model | Size | Author, CH32V002 (cycles) | LP-MSPM0G3507, 32 MHz | LP-MSPM0G3507, 80 MHz | Right (board and Mac) |
|---|--:|--:|--:|--:|---|
| 64-wide CNN, 99.55% | 11.0 kB | 2,736,250 (57.01 ms at 48 MHz) | 2,640,383 (82.5 ms) | 2,750,868 (34.4 ms) | 10 of 10 |
| 16-wide CNN, small, 98.92% | 3.2 kB | 686,490 | 660,687 (20.6 ms) | not run | 10 of 10 |
| 12k fully connected, 99.02% | 12.3 kB | 528,377 | 429,789 (13.4 ms) | not run | 10 of 10 |
| 1k fully connected, 94.22% | 1.1 kB | 99,783 | 89,336 (2.8 ms) | not run | 10 of 10 |

Cycles are the mean of the 10 test digits; the spread from fastest to slowest digit was under 2% for every model. The author's numbers are from his documentation (`docs/documentation_cnn.md`, CH32V002 at 48 MHz, two flash wait states, inner loops run from SRAM); they have not been rerun on that chip. Accuracy percentages are the author's, on the full MNIST test set; only the 10 shipped digits were checked.

Read these as chip plus compiler, not core alone: the CH32V002 numbers come from GCC for RISC-V, the numbers of this repo from TI Arm Clang. At 80 MHz the G3507 needs two flash wait states (set by TI's SysConfig), which is why its cycle count rises 4% over 32 MHz.

**Running the inner loops from SRAM made the G3507 slower**, not faster: 2.95M cycles at 32 MHz and 2.96M at 80 MHz, against 2.64M and 2.75M from flash. The author does this on the CH32V002 to dodge its flash wait states. Not traced further.

Memory, 64-wide CNN: the deepest stack use was 2.2 kB (RAM filled with a pattern before the run, the lowest overwritten word found after). The test digits stay in flash; upstream declares them writable, which would put 2.5 kB of them in RAM, so [`bitnet_run.c`](bitnet_run.c) reads them in as `const` without editing the upstream file.

## The touchscreen demo

[`touch/`](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/ports/bitnetmcu/touch) runs the 64-wide CNN on the Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch), wired to the LP-MSPM0G3507 as in `mspm0g3507/display-test` (13 wires; that page has the pin table, the wiring and why touch uses software I2C).

What you see: "DRAW A DIGIT", a black box, and a red CLEAR button. Draw a digit with a finger and lift it; after half a second the board shows its answer and the time it took. Touch the box again to start a new digit.

The drawing becomes the model's input the way MNIST digits were made: the digit's larger side scaled to 20 of 28 pixels, its center of mass put in the middle, shrunk to 16 x 16 and normalized like the training data. That step takes 1.97M cycles (25 ms at 80 MHz, in software floating point); the answer takes 2,706,506 (34 ms).

Checked: at power-up the board draws each of the 10 test digits into the box, prepares and reads it: 10 of 10 right on the board, the same as on the Mac ([`host/prep_test.c`](host/prep_test.c)). Digits drawn on it on 2026-10-06 were read; how many it got right was not counted. It reads one digit 0 to 9 at a time: no letters, no two-digit numbers, because that is what the model was trained on.

Video: a 40-second phone clip of the demo (filmed 2026-10-06) is on the site with the benchmark, `site/media/bitnetmcu-touch-demo.mp4` (540 x 960, 5.6 MB, no sound).

If touch reads only zeros (touch ID 0 instead of 0x11), the touch pins are still on the chip's I2C block. Switch them to plain GPIO with pull-ups first, as `display_test.c` does.

## Run it yourself

Board benchmark, from `mspm0/` (`<model>`: `cnn_64`, `cnn_16small`, `12k` or `1k`):

```bash
./build.sh g3507 <model> O3 32
```

```bash
./run.sh build-g3507-<model>-O3-32
```

`run.sh` flashes with OpenOCD, fills RAM, runs for 20 seconds and prints the answers, cycles and stack depth. On this board OpenOCD's verify fails on about every other flash and passes on the next try, so the script tries up to three times.

Mac reference answers, from `host/`:

```text
clang -O2 -I../../../third-party/bitnetmcu -DMODEL_H='"BitNetMCU_model_cnn_64.h"' host_main.c -o host_check
```

Touch demo, from `touch/`: `./build.sh`, then flash `out/digit_touch.out` with OpenOCD (`program out/digit_touch.out verify`, then `reset run`).

## LP-MSPM0L1306

Run 2026-10-07: the 64-wide CNN reads all 10 test digits right on the chip with 4 kB of RAM, the same answers as the Mac and the G3507, in 2.65M cycles per digit (82.8 ms at 32 MHz): 3.2% fewer than the author's CH32V002 figure and 0.3% more than the G3507 at the same clock. Deepest stack use 2.2 kB; built with `STACK=3600 ./build.sh l1306 cnn_64 O3 32`, which leaves 396 bytes of RAM. The SRAM variant does not fit.

## The C2000 port

**Short answer.** The 64-wide CNN (the small neural network that reads 16 x 16 handwritten digits) runs on the TI LAUNCHXL-F2800137 (TMS320F2800137, C28x at 120 MHz) with all 10 test digits right, in 2.55M cycles from RAM (21.3 ms) and 2,897,585 from flash (24.1 ms). From RAM that is 6.7 percent fewer cycles than the author's CH32V002 (2,736,250) and 3.4 percent fewer than the LP-MSPM0G3507 at 32 MHz (2,640,383); from flash, 5.9 percent more and 9.7 percent more. At 120 MHz an inference takes a quarter of the MSPM0's 82.5 ms.

Measured 2026-10-07: TI C2000 compiler 25.11.1 at -O4 with `--opt_for_speed=5`, CPU Timer 2 at SYSCLK, stack 1.6k of 2.0k words, answers `3209069277` (the ten predictions in order, the same as the Mac and the MSPM0 boards). The port, its build script and the patches are in `c2000/`.

Three patches were needed because the C28x has no 8-bit type (`char` and `int` are 16 bits), each in the FAQ (`../../c2000-launchpad/FAQ.md`, the three entries that start "BitNetMCU on the C28x"):

1. `1 << shift` in ReLUNorm became `1L << shift`, because a 16-bit `int` overflows past shift 15. The largest shift on these digits is 7, so no answer depended on it.
2. The test digits are written upstream as hex bytes; `0xEC` is 236 as an unsigned byte but -20 as a signed one, so every answer came out `8888808888`. A script writes them as signed values.
3. One buffer is used both as 8-bit and as 32-bit words, so it gets a 2-word alignment. The compiler had already placed it right; the attribute makes it certain.

The arithmetic is unchanged. Next: the same model under FreeRTOS, and the 16-wide and fully connected models on this chip.

License: GPL-3.0, as BitNetMCU ([`third-party/bitnetmcu/LICENSE`](../../third-party/bitnetmcu/LICENSE)). The screen and touch driver is this project's own, from `mspm0g3507/display-test`.
