<!-- experiment
id: E241
board: LP-MSPM0G5187
date: 2026-10-20
status: works
reached: verified
line: A 64-tap FIR, an 8-band filter bank, a max-pool peak detector with comparator and a 64-point DFT, built as fixed-weight network layers with no training, all compiled for the TinyEngine NPU and matched an integer reference byte for byte: the FIR block takes 0.566 ms against 16.824 ms for TI's CPU build (29.7x) and 3.286 ms for one hand-written C channel (5.8x).
stop: none
-->
# Can the LP-MSPM0G5187's TinyEngine NPU run fixed DSP filters faster than the CPU?

**Short answer.** Yes. On 256-sample blocks at 80 MHz the NPU of the LP-MSPM0G5187 runs a 64-tap FIR (four channels) in 0.566 ms and an 8-band filter bank in 0.967 ms. TI's compiler for the CPU takes 16.824 ms and 36.713 ms, so 29.7 and 38.0 times slower. Against hand-written C loops the gain is 21.6 times for four FIR channels and 5.8 times for one. Every image returned the integer reference byte for byte.

Terms: [vocab](../vocab.md). FAQ: [LP-MSPM0G5187](../../mspm0g5187/FAQ.md). Folder with code, models and every run: [mspm0g5187/npu-dsp](../../mspm0g5187/npu-dsp/README.md). NPU is the chip's neural processing unit. NNC is TI's neural network compiler. 1 kB = 1,024 bytes.

**Question.** Many DSP steps are the same math as network layers. Can a hand-built graph with fixed weights run them on the NPU, faster than the CPU?

**Vendor position.** TI's NNC guide (v2.1.2) lists the layers the NPU accepts and says "Support for layer configs that can run on the hardware NPU accelerator will improve as we implement more microcode." A first convolution with one input channel (FCONV) takes any kernel height with width 1 and output channels in multiples of 4, which fits a FIR. Max pooling and 1x1 convolutions are listed. A fully connected layer needs at least 16 inputs with 8-bit weights.

**Expectation, written before the runs.** FIR and bank on the NPU. NPU 0.7 to 1.5 ms and 1.4 to 3 ms; NNC CPU 10 to 20 ms and 20 to 40 ms; ratios 8x to 20x. Weight error near minus 45 to minus 50 dB; end to end 35 to 40 dB. NPU output within 1 count of the reference. A hand-written C FIR near 1.8 ms for one channel, so the gain against it only 1x to 3x.

**What was tried.** Eight graphs, built as ONNX in the integer form of TI's NPU quantization flow, no training: FIR (4 filters, 64 taps), the same with stride 4, an 8-band bank, the bank with a max pool over the block and a 1x1 comparator (four variants), and a 64-point DFT as a fully connected layer. Each was compiled by NNC 2.1.2 for the NPU and as CPU code, linked bare metal with int8 input and output, run on the board with a checked first call and 20 timed calls, and compared with hand-written C for the FIR and the bank.

**How far it got.** Verified. Compiler reports list FCONV, MAXPOOL, PWCONV and FC as offloaded. All 16 images returned 0 differing output bytes.

| Graph | NPU | NNC CPU | NPU against NNC CPU | Hand-written C |
|---|--:|--:|--:|--:|
| FIR, 4 channels | 0.566 ms | 16.824 ms | 29.7x | 12.224 ms; one channel 3.286 ms |
| FIR, 4 channels, stride 4 | 0.185 ms | 4.304 ms | 23.3x | |
| Filter bank, 8 bands | 0.967 ms | 36.713 ms | 38.0x | 22.742 ms |
| Bank, max pool 256, comparator | 0.677 ms | 33.273 ms | 49.1x | |
| DFT, 64 points | 0.195 ms | 0.830 ms | 4.3x | |

**What differed from the expectation.**
- The NPU was faster than predicted (0.566 ms against 0.7 to 1.5): 1.45 multiply-adds per cycle on the FIR, 1.69 on the bank. The ratio to the NNC CPU build (29.7x and 38x) is above the predicted 8x to 20x because that build takes about 20 cycles per multiply-add.
- The hand-written C FIR was slower than predicted: 16 cycles per tap, 3.286 ms, not 1.8 ms. The fair single-filter gain is therefore 5.8x, not 1x to 3x. The NPU cannot run fewer than four output channels.
- End-to-end accuracy was 34.6 to 35.4 dB, just under the 35 to 40 dB predicted; the taps alone gave 42.7 to 53.5 dB. A signal of about 35 counts rms cannot beat about 36 dB through an 8-bit output that floors.
- The 64-point DFT reached only 0.26 multiply-adds per cycle (4.3x).

**What stopped or surprised.** The first compile of the comparator left the 1x1 layer on the CPU. The cause found: a re-scaling shift of exactly 2^0 let the compiler drop a multiply, so the layer pattern no longer matched. A shift of 2^-1 offloaded it (PWCONV listed). A comparator on a signed band output sees positive peaks only.

**Not measured.** Energy, a streaming loop with the 63-sample overlap, CPU work beside the NPU, a second flash of each image, the LP-AM13E230.

Method, accuracy tables, limits and reproduce commands: [README](../../mspm0g5187/npu-dsp/README.md). Results rows for the store: `mspm0g5187/npu-dsp/RESULTS-ROWS.csv`.

**The line.** No limit reached on the chip. Output channels come in multiples of 4, the weights are 8-bit (about 35 dB end to end), and the DFT reaches only 0.26 multiply-adds per cycle.

**Ask.** Has anyone run a fixed-weight filter on this NPU with more than 8-bit accuracy needs, or on the LP-AM13E230? Reports are welcome.

