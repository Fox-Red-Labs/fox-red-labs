# MSPM0 CMSIS-DSP FFT example on the LP-AM263P, unchanged

**It runs.** TI's `cmsis_dsp_fft_q15` example from the MSPM0 SDK (LP-MSPM0G3519 folder, 111-line app) ran on the LP-AM263P's Cortex-R5F on 2026-10-11 with no edit to the app: peak FFT bin 5, the value the example's own comment calls the expected answer, and the input buffer transformed in place. This is the map's Plain C layer made concrete: the maths is CMSIS-DSP, the only board tie is one init call. Matrix id X32 in `portability/matrix.csv`.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes.

## What moved and what was glue

| Part | Lines | Where |
|---|---|---|
| The app, unchanged | 111 | copied over `empty.c` of the MCU+ SDK's `examples/empty` project |
| Shim header `ti_msp_dl_config.h` | 1 line of meaning: `SYSCFG_DL_init()` becomes the MCU+ init sequence the SDK's own `main.c` runs | `ti_msp_dl_config.h` here, beside the app |
| Makefile changes | drop `main.c` (the app has its own), add the CMSIS-DSP include paths, compile 8 CMSIS-DSP sources | `makefile.diff` |

The CMSIS-DSP sources are compiled directly because TI's prebuilt `cmsis.am263px.r5f.ti-arm-clang.release.lib` in MCU+ SDK 26.01.00.08 defines `arm_cfft_radix4_q15` and its neighbours twice (in `TransformFunctions.obj` and in the per-function objects), so linking it fails.

## How it was checked

A fresh DSLite connection reads back zero for every RAM word on this board, even the data section a load wrote a second earlier (the connect sequence resets the view), so the check runs in one CCStudio scripting connection: load, run 3 s, halt, read. `dss_run_read.py` in the parent folder does that:

```bash
/Applications/ti/ccs2101/ccs/scripting/python/run.sh <repo>/portability/everywhere/dss_run_read.py <repo>/am263p/AM263Px_ZCZ_C_S26E0426.ccxml "Cortex_R5_0$" <image> 3 0x70040fbc,2 0x700555d0,8
```

Result 2026-10-11: `0x70040fbc: [5, 4023]` (`gFFTmaxFreqIndex`, `gFFTmaxValue`; addresses from the map file) and the first words of `gDstBuffer` no longer the input samples. Nothing was written to flash; the board's console was not needed.

## Build

```bash
cp -R ~/ti/mcu_plus_sdk_am263px_26_01_00_08/examples/empty <work>/empty
cp <MSPM0 SDK>/examples/nortos/LP_MSPM0G3519/cmsis_dsp/cmsis_dsp_fft_q15/cmsis_dsp_fft_q15.c <work>/empty/empty.c
cp ti_msp_dl_config.h <work>/empty/
patch <work>/empty/am263px-lp/r5fss0-0_nortos/ti-arm-clang/makefile makefile.diff
cd <work>/empty/am263px-lp/r5fss0-0_nortos/ti-arm-clang && make empty.release.out MCU_PLUS_SDK_PATH=~/ti/mcu_plus_sdk_am263px_26_01_00_08 DEVICE=am263px PROFILE=release TOOLS_PATH=~/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0
```
