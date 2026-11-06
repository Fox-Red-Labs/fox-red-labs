# AI assistant notes: fixed DSP filters on the TI LP-MSPM0G5187 NPU

A FIR, a filter bank, a max pool, a comparator and a DFT are ordinary layers to TI's neural network compiler (NNC), all five ran on the NPU here, and the output matched an integer reference byte for byte. Each item below is from runs on one board.

Terms: [docs/vocab.md](../../docs/vocab.md). NPU is the chip's neural processing unit. NNC is TI's neural network compiler. Checked 2026-10-20 with NNC 2.1.2, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07.

## Rules that held

| Step | Do this | Evidence |
|---|---|---|
| FIR | Input `[1,1,N,1]`, `Conv` with kernel `[taps,1]`, valid padding, output channels a multiple of 4 (pad with other filters) | `model/fir64/` |
| Filter bank | Same layer, one output channel per band | `model/bank8/` |
| Decimation | Stride on the convolution | `model/fir64_dec4/` |
| Peak per block | `MaxPool` kernel `[N,1]`, stride `[N,1]` | `model/bank8_trip/` |
| Comparator | 1x1 `Conv` with 127 on the diagonal, bias `-127*threshold`, shift 2^-1 or smaller | `model/bank8_trip/`, `bank8_trips/` |
| DFT | `MatMul` on a rank-2 `[1,64]` input, 64 inputs, 64 cos and sin rows at 127 | `model/dft64/` |
| Layer arithmetic | `Conv`, `Add` bias, `Mul` integer, `Mul` 2^-k, `Floor`, `Clip(-128,127)`; unsigned outputs use `Clip(-255,255)`, `Relu`, `Clip(0,255)` | `host/make_models.py` |
| Compile | `--target="c, ti-npu skip_normalize=true output_int=true"` for the NPU, `"c, ti-npu type=soft ..."` for the CPU, `--target-c-mcpu=cortex-m0plus` | `nnc.sh` |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Compile log lists `qnn.conv2d`, `cast`, `floor`, `clip` as not offloaded after a 1x1 layer | A re-scaling shift of exactly 2^0 lets the compiler drop a multiply, so the layer pattern no longer matches | Use a shift of 2^-1 or smaller and fold the factor into the weights | Log lists `PWCONV` under offloaded layers |
| One FIR channel will not map | The layer table needs output channels in multiples of 4 | Run four filters and use one | `FCONV` offloaded |
| Program waits forever after a debugger reset | A reset during an NPU run wedges the NPU | Flash with DSLite so the image starts itself, then halt and read (`run.sh`) | Result block magic `d5b00001` |
| Comparator output is 0 for a signal that clearly exceeds the trip | The band output is signed and the pool took the peak of a negative lobe, or the threshold is above the peak | Pool over at least one period; read the peak count first | The band peaks are in the printed result block |

## Numbers (80 MHz, 256-sample block)

| Graph | NPU | NNC CPU | Hand C |
|---|--:|--:|--:|
| FIR 64 taps, 4 channels | 0.566 ms | 16.824 ms | 12.224 ms (1 channel 3.286 ms) |
| Filter bank, 8 bands | 0.967 ms | 36.713 ms | 22.742 ms |
| Bank, max pool, comparator | 0.677 ms | 33.273 ms | |
| DFT 64 | 0.195 ms | 0.830 ms | |

End-to-end accuracy against a float filter is about 35 dB for a mid-scale signal (8-bit output). Details and limits: `README.md`.
