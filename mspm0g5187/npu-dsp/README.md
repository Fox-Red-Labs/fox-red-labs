# Fixed DSP filters on the TinyEngine NPU of a TI LP-MSPM0G5187

A 64-tap FIR, an 8-band filter bank, a max-pool peak detector, a comparator and a 64-point DFT all compile as ordinary network layers with fixed, untrained weights, and every one runs on the NPU. On 256-sample blocks at 80 MHz the NPU takes 0.57 ms for the FIR (four channels) and 0.97 ms for the 8-band bank. TI's compiler for the CPU takes 16.8 ms and 36.7 ms for the same graphs, so 30 times and 38 times. Against hand-written C loops the gain is 22 times (four FIR channels), 24 times (8 bands) and 5.8 times (one FIR channel, the fair comparison for a single filter). All 16 images return the integer reference byte for byte.

Terms: [vocab](../../docs/vocab.md). NPU is the chip's neural processing unit. NNC is TI's neural network compiler. FIR is a finite impulse response filter. Board facts: [AGENTS.md of the board](../AGENTS.md). Experiment page: [fixed DSP on the NPU](../../docs/experiments/2026-10-20-mspm0g5187-npu-fixed-dsp.md). Checked 2026-10-20 on one LP-MSPM0G5187 (probe <probe serial>), MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, NNC 2.1.2.

## What maps to the NPU

Source for the layer rules: TI Neural Network Compiler for MCUs User's Guide v2.1.2, pages "Layer Patterns Supported on the NPU" and "Layer Configurations Supported on the NPU" (software-dl.ti.com/mctools/nnc/mcu/users_guide/). The guide says of its tables: "Support for layer configs that can run on the hardware NPU accelerator will improve as we implement more microcode." Result column is what the compiler's offload report and the board showed.

| DSP step | Layer | TI rule (guide tables) | Result here |
|---|---|---|---|
| FIR, taps as weights | FCONV: one input channel, kernel height any, kernel width 1, output channels a multiple of 4, 2, 4 or 8-bit weights | one FIR needs 4 output channels | On the NPU. One filter alone is not allowed, so four filters run |
| Filter bank | FCONV with 8 output channels | same | On the NPU |
| Decimation | FCONV with stride (stride height any) | same | On the NPU (stride 4 tried) |
| Peak or maximum over time | MAXPOOL: kernel height any, width 1, channels a multiple of 4 | same | On the NPU, kernel 256 and 16 tried |
| Trip point, comparator | PWCONV (1x1 convolution, channels a multiple of 4) followed by the bias, scale, shift, floor and clip sequence | same | On the NPU, with one trap below |
| DFT or DCT | FC: 8-bit input, 8-bit weights, at least 16 inputs | same | On the NPU (64 in, 64 out) |
| Input scaling, float to int8 | add, multiply, floor, clip at the head | not an NPU layer | CPU; removed with `skip_normalize=true`, the application passes int8 |
| Reshape | reshape | not an NPU layer | Listed as not offloaded; no cost seen |
| Absolute value, square, IIR feedback | none | no such layer | Not possible as a layer; rectify with a ReLU clip or take peaks with max pool |

## Method

1. `host/make_models.py` writes each graph in the integer-in-float ONNX form that TI's NPU quantization flow exports (read from the occupancy model of this repo): `Add`, `Mul`, `Mul` by a power of two, `Floor`, `Clip`, around `Conv` or `MatMul` with integer weights. Taps are windowed-sinc (Hamming, 64 taps), scaled by the largest power of two that keeps every tap within plus or minus 127.
2. The same arithmetic is computed in plain integer numpy (the reference) and through onnxruntime. They agree on every output of every graph.
3. NNC 2.1.2 compiles each graph for the NPU and as CPU code (`ti-npu type=soft`) with `skip_normalize=true output_int=true`, on a Linux machine (`nnc.sh` runs it on a GitHub Linux runner; NNC has no Intel Mac build).
4. `build.sh` links one bare-metal image per graph and target. `firmware/main.c` runs the block once, compares with the reference, then times 20 calls with the SysTick cycle counter. `firmware/cbase.c` is the hand-written C baseline. `run.sh` flashes with DSLite, lets the image start itself (no debugger reset, which can wedge the NPU), halts and reads the result block.

Test blocks are 256 outputs from 319 input samples (63 samples of history). Signals are tones plus a few counts of noise; `host/make_models.py` holds them.

## Numbers

Timing boundary: the model call only, int8 in and out. Median of 20 calls, one erase, flash and run per image. The spread of the 20 calls is under 0.1 percent on every image.

| Graph | Multiply-adds | NPU | NNC CPU | NPU against NNC CPU | Hand-written C | NPU against hand C |
|---|--:|--:|--:|--:|--:|--:|
| FIR 64 taps, 4 channels | 65,536 | 0.566 ms | 16.824 ms | 29.7x | 12.224 ms (4 ch), 3.286 ms (1 ch) | 21.6x, 5.8x |
| FIR 64 taps, 4 channels, stride 4 | 16,384 | 0.185 ms | 4.304 ms | 23.3x | not built | |
| Filter bank, 8 bands | 131,072 | 0.967 ms | 36.713 ms | 38.0x | 22.742 ms | 23.5x |
| Bank, max pool 256, comparator | 131,072 | 0.677 ms | 33.273 ms | 49.1x | not built | |
| Bank, max pool 16 twice, comparator | 131,072 | 0.694 ms | 33.318 ms | 48.0x | | |
| Bank, max pool 16, comparator (128 outputs) | 131,072 | 0.713 ms | 33.668 ms | 47.2x | | |
| DFT 64 points, 64 outputs | 4,096 | 0.195 ms | 0.830 ms | 4.3x | not built | |

Multiply-adds per NPU cycle: 1.45 on the FIR, 1.69 on the bank, 0.26 on the DFT. The hand-written C loop takes 16 cycles per tap.

Image sizes (whole image with harness and NPU library): FIR 7.9 kB flash and 4.9 kB RAM; bank 8.3 kB and 8.1 kB; bank with comparator 6.9 kB and 6.1 kB; DFT 9.9 kB and 2.6 kB. RAM includes a 2 kB stack. The NNC CPU build of the bank needs 20.4 kB of RAM and still fits.

The bank with a max-pool and comparator is faster than the bank alone (0.68 ms against 0.97 ms): a likely cause is that the pool consumes the band output on the NPU, so the 2 kB result is never laid out for the CPU (not tested).

### Accuracy (Python, on the test signals)

| Case | Result |
|---|---|
| Output against the integer reference, all 16 images | 0 of the output bytes differ, maximum difference 0 |
| FIR channels, int8 taps in float arithmetic against float taps | 42.7 to 53.5 dB |
| FIR channels, whole int8 pipeline against the float filter | 34.6 to 35.4 dB, signal about 35 counts rms |
| Filter bank bands, whole pipeline | 17 to 30 dB, signals only 4 to 19 counts rms |
| DFT, whole pipeline | 25.5 dB |
| Comparator, trip at 15 counts, band peaks 6 to 25 | bands with a peak above 15 gave a nonzero output, the others 0, on the board and in Python |

The ceiling of 8-bit weights alone is near 48 dB, and the taps landed around it. The whole-pipeline figure is lower because the output is also 8-bit and the compiler's re-scaling floors, which adds about half a count of error: a signal of 35 counts rms cannot do better than about 36 dB. A larger signal improves it by 6 dB per doubling.

## Limits

- Output channels must be a multiple of 4. One FIR costs four channels of work (here four cutoffs). Against a hand-written one-channel FIR the gain falls from 22 times to 5.8 times.
- Weights and signals are 8-bit. About 35 dB end to end is what to expect for a mid-scale signal; this does not replace a 16-bit or floating-point filter where dynamic range matters.
- A shift of exactly 2^0 in the layer's re-scaling step made the compiler drop a multiply and leave the 1x1 comparator on the CPU (`qnn.conv2d`, `cast`, `floor` and `clip` listed as not offloaded). A shift of 2^-1 fixed it, with the comparator output halved.
- A comparator on a signed band output sees positive peaks only. A level meter needs the peak over a block (max pool) or a rectifier built from two output channels.
- Block processing: the CPU must copy the last 63 samples in front of each block. Not built or timed here.
- The inference call blocks while the NPU works (the harness spins). Whether the CPU can do useful work meanwhile, and the energy per block, were not measured.
- One flash and run per image. The 20 calls inside each run agree to 0.1 percent, but a second flash was not done.
- Only this chip. The LP-AM13E230 (same layer rules, different NPU clock) was not run.

## Files

`host/make_models.py` (graphs, reference, accuracy), `host/make_rows.py`, `nnc.sh`, `build.sh`, `run.sh`, `runall.sh`, `firmware/main.c`, `firmware/cbase.c`, `model/` (ONNX files, vectors, offload reports) and `RESULTS-ROWS.csv`.

Reproduce: `python3 host/make_models.py <dir>`, `./nnc.sh <dir>`, `./build.sh`, `./runall.sh 1` (the Python environment needs onnx, onnxruntime and numpy).

## Porting lessons

- Fixed DSP compiles as ordinary network layers with untrained weights: a FIR is a convolution with the taps as weights, a peak detector is a max pool, a DFT is a fully connected layer. Compile with `skip_normalize=true output_int=true` and pass int8 buffers.
- Output channels must be a multiple of 4, so one FIR costs four channels of work. Against a hand-written one-channel loop the gain is 5.8 times, not 22 times.
- A re-scaling shift of exactly 2^0 made the compiler drop a multiply and leave the 1x1 comparator layer on the CPU. A shift of 2^-1 keeps it on the NPU, with the output halved.
- Eight-bit weights and signals give about 35 dB end to end for a mid-scale signal. The layer does not replace a 16-bit or floating-point filter where dynamic range matters.
- Let the image start itself after flashing: a debugger reset can wedge the NPU. The DFT reached only 0.26 multiply-adds per cycle, against 1.45 to 1.69 for the FIR and filter bank.
