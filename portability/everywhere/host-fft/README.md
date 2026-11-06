# MSPM0 CMSIS-DSP FFT example on the SK-TDA4VM Linux processor, unchanged

**It runs.** TI's `cmsis_dsp_fft_q15` example from the MSPM0 SDK built with the gcc on the SK-TDA4VM's own Linux (Cortex-A72, aarch64) and printed `gFFTmaxFreqIndex=5 gFFTmaxValue=4023 PASS` on 2026-10-11, the same answer the example's comment expects, the same numbers the LP-AM263P's R5F and this Mac (x86_64) gave. The app file is untouched. Matrix id X33 in `portability/matrix.csv`.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes.

## What travels

| Part | Lines | Role |
|---|---|---|
| `app.c`, the example as shipped | 111 | unchanged |
| 10 CMSIS-DSP sources from the MCU+ SDK's copy (`arm_cfft_q15`, `arm_cfft_radix4_q15`, both bit-reversal files, `arm_cmplx_mag_q15`, `arm_max_q15`, `arm_sqrt_q15`, the common tables and const structs) and its `Include/` | TI's own | the maths |
| `cmsis_compiler.h` (here) | 14 | the plain C forms of the Cortex-M intrinsics CMSIS-DSP falls back to when `ARM_MATH_DSP` is not defined: saturate, count leading zeros, alignment, and `__BKPT` routed to a function |
| `ti_msp_dl_config.h` and `host.c` (here) | 3 and 8 | the host harness: the app's one init call arms a one-second alarm that prints the result globals and exits, because the app ends in `while (1)`; the app's own failure path (`__BKPT`) prints FAIL |

The harness is a test fixture, not a shim: it adds behavior (the alarm) so a program written to run forever can report. It touches no app line.

## How to repeat it

On any host with a C compiler:

```bash
mkdir -p w/inc w/src && cd w
cp <this folder>/*.h inc/ && cp <this folder>/host.c src/
cp <MSPM0 SDK>/examples/nortos/LP_MSPM0G3519/cmsis_dsp/cmsis_dsp_fft_q15/cmsis_dsp_fft_q15.c src/app.c
D=<MCU+ SDK>/source/cmsis/DSP; cp -R $D/Include/* inc/
for f in TransformFunctions/arm_cfft_q15.c TransformFunctions/arm_cfft_radix4_q15.c TransformFunctions/arm_bitreversal.c TransformFunctions/arm_bitreversal2_c.c ComplexMathFunctions/arm_cmplx_mag_q15.c StatisticsFunctions/arm_max_q15.c FastMathFunctions/arm_sqrt_q15.c CommonTables/arm_common_tables.c CommonTables/arm_const_structs.c; do cp $D/Source/$f src/; done
cc -O2 -std=gnu99 -Iinc -w src/*.c -o fft && ./fft
```

On the SK-TDA4VM with no network: the same tree as a tar.gz, base64, pasted through the serial console at 115200 (`stty -echo; cat > fft.b64 << 'B64EOF'`, 256-byte writes 25 ms apart, about 170 s for 1.4 MB), `md5sum` compared with the Mac's, then the same `gcc` line.
