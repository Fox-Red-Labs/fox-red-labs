# TI LP-MSPM33C321A LaunchPad: benchmarks and a changed blink from a Mac terminal

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer.** With the tools installed, a copied TI example went from first command to a verified changed blink in 19 seconds on 2026-10-08 (build 4.8 s, flash 11.3 s). The Cortex-M33 runs CoreMark at 140.3 per second at 32 MHz (4.38 per MHz) and 544.1 per second at 160 MHz (3.40 per MHz), and TI's post-quantum ML-KEM-768 encapsulation takes 1.72 million cycles. Every number is a row in `data/results/mspm33c321a.csv`.

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes.

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `blink.sh`; `prebuild.sh` (builds every target that needs no board; it writes outside the repository). Folders: `bench/` benchmark sources, `motor/` motor-control builds, `tools/` helper scripts for this board. Results are rows in `data/results/mspm33c321a.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add mspm33c321a tools data third-party/coremark portability/fom third-party/mlkem-native third-party/mldsa-native
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

The LP-MSPM33C321A is a prototype LaunchPad sold in limited quantity (TI.com). Out of the box the OLED shows TI's logo and the S2 button plays a short video from the QSPI flash. That demo is not in the SDK, so save the flash before you change anything (step 2 below).

| Part | Fact |
|---|---|
| Chip | MSPM33C321A: Arm Cortex-M33 at up to 160 MHz, floating-point unit, TrustZone |
| Memory | 1 MB flash, 256 kB RAM; 16 MB QSPI flash (holds the demo video; the debugger cannot read it) |
| Debugger | Onboard XDS110 with EnergyTrace |
| Display | 0.91 inch OLED |
| LEDs and buttons | RGB LED, two buttons (S2 plays the demo video) |
| Headers | 80-pin BoosterPack headers |

Source for every row: the board description in this README as checked on 2026-10-08. Numbers below were measured with command-line tools on the setup in "Where these facts come from".

## Ten-minute path

You need: the board and the USB cable from the box, a Mac with the tools above installed in `/Applications/ti`. Everything runs in a terminal.

1. Plug in the board and find its probe serial (the XDS110 is listed with its serial number):

```bash
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
```

2. Identify the chip and save the flash. This reads only; the script writes the connection file `MSPM33C321A_<serial>.ccxml`, then you read main flash, data flash and the boot configuration (twice, to prove the read works):

```bash
mspm33c321a/tools/identify.sh <serial>
```

```text
DSLite=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
$DSLite memory -c mspm33c321a/MSPM33C321A_<serial>.ccxml -r 0x10000000,262144 -s 32 -o main.bin
```

Repeat with `-r 0x80000000,8192` for data flash and `-r 0x80101800,512` for the boot configuration area (read only: never write it). The 16 MB QSPI flash (JEDEC ID C2 20 18, read by the SDK QSPI driver) that holds the demo video cannot be read by the debugger, so do not run TI's `qspi_flash_readback` examples, which erase part of it.

3. Build TI's blink example with a 250 ms period and flash it (one command, it prints the build and flash seconds):

```bash
mspm33c321a/blink.sh 250 <serial>
```

4. Check that the pin toggles by reading the GPIO output register over the probe, or watch the blue LED blink at twice TI's rate:

```bash
mspm33c321a/tools/led_check.sh <serial>
```

5. To put TI's demo back, write the saved flash back with DSLite and read it again; the comparison must show no difference.

## One-command build

```bash
mspm33c321a/prebuild.sh --wait
```

It builds TI's blink, CoreMark at 32 and 160 MHz, the four room-occupancy models, the control-loop step and (without `--no-sweep`) every SDK example, into the folder named by `OUT` (default: a folder outside the repository). It waits for a quiet Mac and installs nothing. The shorter benchmark builds are `tools/build_timing.sh`, `build_dhrystone.sh`, `build_crypto.sh` and `build_adc.sh`; `tools/run_image.py` flashes one image, waits and reads its result variables back over the probe.

## What was measured

Each row names its test in `data/results/mspm33c321a.csv`, with the recipe and conditions.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Blink by command line, first command to pin read showing the toggle | 19 s (build 4.8 s, flash 11.3 s) | 2026-10-08; LED not watched by eye | measured | `time_to_blink`, `blink_build`, `blink_flash` in `data/results/mspm33c321a.csv` |
| CoreMark, 32 MHz | 140.3 per second (4.38 per MHz) | TI Arm Clang -O3, bare metal, flash needs 0 wait states | measured | `coremark`, `coremark_per_mhz` (ticlang-bare-o3-32mhz) |
| CoreMark, 160 MHz | 544.1 per second (3.40 per MHz) | -O3, flash needs 2 wait states, code cache on; wait-state registers read | measured | `coremark`, `coremark_per_mhz` (ticlang-bare-o3) |
| CoreMark per MHz change from 32 to 160 MHz | falls 22 percent | computed from the two rows above | computed | `data/results/mspm33c321a.csv` |
| Dhrystone, 32 MHz and 160 MHz | 1.22 and 0.88 DMIPS per MHz | rules followed | measured | `dhrystone` (ticlang-bare-o3-noinline-32mhz, ticlang-bare-o3-noinline) |
| ML-KEM-768 keygen, encaps, decaps | 1.48M, 1.72M, 1.99M cycles | TI's examples, 32 MHz | measured | `pqc_mlkem768_keygen`, `pqc_mlkem768_encaps`, `pqc_mlkem768_decaps` |
| ML-DSA-44 keygen, sign, verify | 2.73M, 23.2M, 3.47M cycles | TI's examples, 32 MHz | measured | `pqc_mldsa44_keygen`, `pqc_mldsa44_sign`, `pqc_mldsa44_verify` |
| ECDSA P-256 sign | 5.21M cycles | TI's example, 32 MHz | measured | `ecdsa_p256_sign` |
| Room-occupancy neural nets | all four match 20 of 20 test vectors; the 8k-parameter one runs in 7.4 ms | 160 MHz | measured | `ai_occ_latency` (occ_cnn_8k-c-o3), `ai_occ_fit` |
| ADC sample rate | 5.0 million samples a second | TI's setting, DMA in single-transfer mode | measured | `adc_max_rate` |
| ADC sample rate in the data sheet | 9.4 million samples a second | not reached on this bench | claim | `adc_max_rate` (ti-example-single-block, failed) |
| AES-256 hardware against tiny-AES-c | 41.7 times faster | 1 kB, ECB | measured | `aes_speedup` (1kb-ecb-ticlang-o2-keyin) |
| CRC module against a table | 2.75 times faster | | measured | `crc_speedup` |
| Education BoosterPack light sensor | answers on I2C at address 0x44 (PB13 SCL, PB12 SDA) | manufacturer ID 0x5449, device ID 0x3001 | measured | I2C scan, 2026-10-08 |
| Full 128 x 128 screen fill | 35 ms | 16 MHz SPI clock | measured | `display_full_frame` (edumkii-st7735s-128x128-16mhz) |
| SDK examples | 258 of 258 build; 178 flashed and run, 177 left the debugger connected | MSPM33 SDK 1.04.00.00 | measured | `examples_built`, `examples_ran_clean` |

## If you see this

- If you see `Device Connection Error ... low power mode` in DSLite after TI's FreeRTOS `blink_led` example, pulse the board's reset line from the probe: `xds110reset -s <serial> -a toggle -d 200` (next to `xdsdfu` in CCStudio's `xds110` folder). No unplugging was needed on 2026-10-08.
- If you see `Verification failed: Values at address 0x00000020 do not match` when flashing a FreeRTOS build, change `-T./mspm33c321a.cmd` to `./mspm33c321a.cmd` in the example's makefile. TI Arm Clang 5.1.1 ignores the `-T` form (it warns `Case insensitivity of options has been deprecated; T must be written as t`), so the code links at address 0x20. Then pulse the reset line as above and flash a good image.
- If you see `ERROR! Must execute for at least 10 secs` or a garbage CoreMark result at 160 MHz, pick the iteration count so the run lasts between 10 s and 26 s. The 32-bit cycle counter wraps after 26.8 s at 160 MHz.

The rest of the errors, with causes: [FAQ.md](FAQ.md).

## Limits

No sleep or run current figures: they need the board's own EnergyTrace with jumpers lifted. The blink LED was read through the debugger, not watched by eye. FreeRTOS: a task switch round trip takes 367 cycles at 32 MHz, and 397 tasks run with the heap raised from 8 kB to about 248 kB (`rtos_task_switch`, `rtos_max_tasks` in `data/results/mspm33c321a.csv`).

## Where these facts come from

Checked 2026-10-08 on an Intel Mac running macOS 15.7, with CCStudio 21.0.1 (used only for its compiler and DSLite), TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, UniFlash 9.6.0 and MSPM33 SDK 1.04.00.00, probe firmware 3.0.0.43. Every number comes from command-line runs. Files: `data/results/mspm33c321a.csv`, `mspm33c321a/AGENTS.md`, `mspm33c321a/FAQ.md`.
