# TI LP-MSPM0G5187 LaunchPad: NPU speed against the CPU, and the wake-word demo

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer.** On the 8k-parameter room-occupancy model the NPU takes 2.12 ms and the CPU takes 35.27 ms at 80 MHz: 16.7 times faster, with all 20 test vectors giving the right class. The gain is 3.0 times on a 500-parameter model and 1.9 times on a 30k-parameter model. The board shipped with a 424 B program already on it, and a command-line flash and read of the chip works from a Mac with probe firmware 3.0.0.38, even though TI's tool prints a warning on every call. Measured 2026-10-08 and 2026-10-09 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0. Also measured 2026-10-09: AES, CRC, Embench, Dhrystone, FFT and the UART loop ([results](RESULTS-mspm0g5187.md)).

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. **NPU** is the chip's neural processing unit. **NNC** is TI's neural network compiler. **DSLite** is the command-line flasher inside UniFlash. **CoreMark** is a standard CPU speed test.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-mspm0g5187.md` (the measured results). Folders: `alarm/` the alarm listener, `radio/` the sub-1 GHz radio demos, `wakeword/` the wake-word demo. Results are rows in `data/results/mspm0g5187.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add mspm0g5187 tools data mspm0g3507/mcu-bench third-party/coremark
```

`docs/` is left out on purpose (mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.

## The board

The LP-MSPM0G5187 LaunchPad development kit is the only MSPM0 family member with I2S audio and USB.

| Part | Fact | Read from |
|---|---|---|
| Chip | MSPM0G5187: Arm Cortex-M0+ at 80 MHz | `AGENTS.md` |
| Memory | 128 kB flash, 32 kB RAM | `AGENTS.md` |
| NPU | TinyEngine NPU that TI rates at 2.56 billion operations a second at 80 MHz | TI's published rating, `AGENTS.md` |
| Debugger | XDS110 with EnergyTrace; Rev. A board; shipped on probe firmware 3.0.0.38 | `FAQ.md` |
| Audio, storage, USB | A microphone feeding a PCM1809 audio ADC, a microSD slot, a second USB-C port for the chip's own USB | `AGENTS.md` |
| Header | A 40-pin header | `AGENTS.md` |
| Shipped program | A 424 B program was on the board; TI's SDK has no source for it | `mspm0g5187/README.md` |

## Ten-minute path (measured, Intel Mac)

You need the board, a USB-C port or an adapter for the USB-A cable in the box, and a Mac with CCStudio 21.0.1 (it carries DSLite and the compiler) and the MSPM0 SDK 2.11.00.07. Back up the flash first: TI's SDK has no source for the program the board ships with, so the backup is the only copy.

1. Copy TI's blink example and build it. About 4 seconds.

```bash
cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0G5187/driverlib/gpio_toggle_output .
```

```bash
../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
```

2. Flash it with UniFlash's command line. About 5 seconds.

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c MSPM0G5187_MG020001.ccxml -e -f -v -u gpio_toggle_output/ticlang/gpio_toggle_output.out
```

Replace `MG020001` with your probe's serial number (`xdsdfu -e` lists it). The red, green and blue LEDs blink. Change `DELAY` in `gpio_toggle_output.c` and flash again to see the rate change.

## One-command build

```bash
./prebuild.sh
```

Builds blink, CoreMark, the four occupancy models on the CPU and on the NPU and the handwriting models into `out/mspm0g5187/`. It waits for a quiet machine first. It does not flash or run anything on the board.

## What was measured

Measured 2026-10-08 to 2026-10-10 on one board. Also measured 2026-10-09: Dhrystone, FFT and Embench ([results](RESULTS-mspm0g5187.md)).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Chip: 80 MHz, 128 kB flash, 32 kB RAM, NPU 2.56 billion operations a second | as listed | Vendor figures | claim | TI's published ratings |
| Blink build, TI makefile | 4.0 s median of 3 | SysConfig and compile | measured | `data/results/mspm0g5187.csv`, test `blink_build` |
| Blink, one command (`frl run`: build, flash, verify the asked pattern on PA17) | 10.1 s median of 3 (9.4 to 10.5) | Warm run, SysConfig 1.28.0 named with `FRL_SYSCONFIG` (route validated on 1.26.3); image 536 B of flash | measured 2026-11-08 | `data/results/mspm0g5187.csv`, test `time_to_blink` (cli) |
| Blink flash, DSLite | 4.7 s median of 3 | UniFlash command line | measured | `data/results/mspm0g5187.csv`, test `blink_flash` (dslite) |
| Blink flash, OpenOCD | 20.7 s once | OpenOCD also works with the G3507 board file | measured | `data/results/mspm0g5187.csv`, test `blink_flash` (openocd) |
| CoreMark, TI Arm Clang `-O3`, 80 MHz | 182.79 (2.285 per MHz) | `-O2` 122.62, `-Os` 110.00, `-Oz` 98.50; at 32 MHz `-O3` 78.80 (2.462 per MHz) | measured | `data/results/mspm0g5187.csv`, test `coremark` |
| Interrupt latency | 36 to 40 cycles | 7 cycles more than the G3507's best; unexplained | measured | `data/results/mspm0g5187.csv`, test `irq_latency` |
| ADC with DMA, TI example as shipped | 0.547 Msps | 1k samples in 1.87 ms; data sheet rates 1.6 Msps; unexplained | measured | `data/results/mspm0g5187.csv`, test `adc_max_rate` |
| AES-128 and CRC-32 on 1 kB | 45.5 and 8.1 times faster than software | Hardware against software | measured | `data/results/mspm0g5187.csv`, tests `aes_speedup`, `crc_speedup`; [results](RESULTS-mspm0g5187.md) |
| UART loop | clean to 10.67 Mbaud | Bad patch near 5.5 and 6.0 Mbaud | measured | `data/results/mspm0g5187.csv`, test `uart_max_clean` |
| Room occupancy 8k model, CPU against NPU | CPU 35.27 ms, NPU 2.12 ms: 16.7 times faster; all 20 test vectors right | 80 MHz | measured | `data/results/mspm0g5187.csv`, test `ai_occ_latency`; [experiment](../docs/experiments/2026-10-09-mspm0g5187-npu-vs-cpu.md) |
| NPU gain on the other models | 3.0 times on 500 parameters, 1.9 times on 30k | Ratio of the CPU and NPU times in the same CSV | computed | `data/results/mspm0g5187.csv`, test `ai_occ_latency` |
| SDK example sweep | 88 of 113 candidates ran, 79 clean, 7 hang at boot waiting for a clock | See `RESULTS-mspm0g5187.md` | measured | `data/results/mspm0g5187.csv`, tests `examples_built`, `examples_ran_clean` |

## If you see this

- If you see `Warning: A firmware update is required for the XDS110 probe` on every DSLite command, no update is needed here: the board ships with probe firmware 3.0.0.38 and DSLite still identifies the chip, reads and writes flash and verifies it. Update only if a command fails instead of warning.
- If you see a flashed TI example do nothing and the debugger stopped in `SYSCFG_DL_SYSCTL_CLK_init`, the generated start-up code waits forever for a clock-ready bit this board never sets. Open `ti_msp_dl_config.c`, take `DL_SYSCTL_CLK_STATUS_LFOSC_GOOD` out of the `while` mask in that function, or pick a different example.
- If you see the Sensors BoosterPack demo print `scan=none`, unplug USB, press the BoosterPack straight down until its pin 1 sits in the kit's J1 pin 1, and plug in again. A seated stack prints `scan=40,44`.

The rest, with causes, are in [FAQ.md](FAQ.md).

## Limits

7 of the 88 examples that ran hang at boot waiting for a clock (`RESULTS-mspm0g5187.md`). The onboard microphone needs a second USB-C cable.

## Where these facts come from

Checked 2026-10-08 to 2026-10-10 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 and xPack OpenOCD 0.12.0-7, on one LP-MSPM0G5187 Rev. A (probe firmware 3.0.0.38). TI.com price read 2026-10-04.

Files: `data/results/mspm0g5187.csv`, `RESULTS-mspm0g5187.md`, `AGENTS.md`, `FAQ.md`.
