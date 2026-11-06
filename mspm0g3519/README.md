# TI LP-MSPM0G3519 LaunchPad from a Mac, compared with the LP-MSPM0G3507

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Measured on 2026-10-08.** The board builds and flashes from a Mac in 8 seconds and holds the LP-MSPM0G3507's numbers at 80 MHz: CoreMark 182.73 per second at -O3 (2.284 per MHz), the same handwriting model in the same 2.64M cycles. TI's Zephyr blinky for it builds and flashes but does not run (see [FAQ](FAQ.md)). Not run: Zephyr benchmarks, power and the AI cost runs. Every number and its recipe is in `data/results/mspm0g3519.csv`; the write-up is [RESULTS-mspm0g3519.md](RESULTS-mspm0g3519.md).

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. **CoreMark** is a standard CPU speed test; **DSLite** is TI's command-line flasher; **OpenOCD** is an open-source debug and flash tool; **Zephyr** is an open-source real-time operating system; **EnergyTrace** is TI's current measurement on the debug probe.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-mspm0g3519.md` (the measured results). Results are rows in `data/results/mspm0g3519.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add mspm0g3519 tools data mspm0g3507/mcu-bench third-party/coremark
```

`docs/` is left out on purpose (mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.

## The board

The LP-MSPM0G3519 LaunchPad development kit carries an MSPM0G3519. It is the core of the LP-MSPM0G3507 with four times the RAM and flash, so everything that ran on the G3507 here runs again on it. Sources: data sheet SLASFA2C and user guide SLAU947, read 2026-10-07.

| Part | Fact | Read from |
|---|---|---|
| Chip | MSPM0G3519: Arm Cortex-M0+ at 80 MHz | data sheet SLASFA2C |
| Memory | 512 kB of dual-bank flash, 128 kB of RAM | data sheet SLASFA2C |
| On the chip | Two 4 Msps ADCs, a 12-bit DAC, a math accelerator, AES with key storage, a true random-number generator and two CAN-FD ports | data sheet SLASFA2C |
| Debugger | XDS110 with EnergyTrace (it measures current); a new board can ship on probe firmware 3.0.0.31 | user guide SLAU947; `FAQ.md` |
| LEDs and buttons | An RGB LED and a red LED, three buttons | user guide SLAU947 |
| Clocks and header | 40 MHz and 32 kHz crystals, an 80-pin BoosterPack header | user guide SLAU947 |
| Serial | UART0 on PA10 and PA11; the Zephyr backchannel uses jumpers J12 and J13 on the XDS side | `RESULTS-mspm0g3519.md` |

## Ten-minute path (measured on an Intel Mac)

You need the board, its USB cable (micro-B), and a Mac with CCStudio 21.0.1 (it carries TI's command-line tools) and the MSPM0 SDK 2.11.00.07.

1. Check the probe firmware. A new board can ship with 3.0.0.31, and TI's command line then refuses to connect (see the [FAQ](FAQ.md) for the two commands that fix it).

```bash
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
```

2. Copy TI's blink example and build it. It took 3.2 seconds here.

```bash
cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0G3519/driverlib/gpio_toggle_output .
```

```bash
../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
```

3. Flash it with TI's UniFlash command line and a connection file for your probe serial (`MSPM0G3519_MG000001.ccxml` is this repo's file; change the serial). It took 4.6 seconds.

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c MSPM0G3519_MG000001.ccxml -e -f -v gpio_toggle_output/ticlang/gpio_toggle_output.out
```

4. Change `DELAY` in `gpio_toggle_output.c`, rebuild and flash again. The LED blinks at the new rate.

OpenOCD also flashes this chip, in 0.6 seconds. Zephyr 4.4.2's upstream board builds and flashes, but its blinky stops in an interrupt right after reset; use the SDK path above first.

## One-command build

```bash
./prebuild.sh
```

Builds blink, CoreMark at four flag levels and two clocks, the occupancy and handwriting models, every SDK example's build pass and the Zephyr images into `out/mspm0g3519/`. It waits for a quiet machine first. It does not flash or run anything on the board.

## What was measured

2026-10-08, one board, code run from flash. Not run: Zephyr benchmarks beyond CoreMark, power and the AI cost runs. The write-up with every recipe is [RESULTS-mspm0g3519.md](RESULTS-mspm0g3519.md).

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Chip: 80 MHz, 512 kB flash, 128 kB RAM | as listed | Vendor figures | claim | data sheet SLASFA2C |
| Build of TI's blink | 3.2 s median (4.1, 3.2, 3.0) | `sdkmake.sh`, fresh copy each run | measured | `data/results/mspm0g3519.csv`, test `blink_build` (cli) |
| Flash with UniFlash | 4.6 s median (4.8, 4.6, 4.6) | `dslite.sh --mode flash -e -f -v` | measured | `data/results/mspm0g3519.csv`, test `blink_flash` (dslite) |
| Flash with OpenOCD | 0.6 s | Program, verify, reset; the first run after a UniFlash load took 20.6 s | measured | `data/results/mspm0g3519.csv`, test `blink_flash` (openocd) |
| Zephyr blinky build and flash | 51.5 s and 4.5 s median; 11.8 kB flash, 4.0 kB RAM | Does not run, see the FAQ | measured | `data/results/mspm0g3519.csv`, tests `blink_build`, `blink_flash` (zephyr, west), `blink_flash_bytes`, `blink_ram_bytes` |
| CoreMark, -O3, 80 MHz | 182.73 per second (2.284 per MHz) | Three identical runs, validated by CRC; the LP-MSPM0G3507 gave 183.61 | measured | `data/results/mspm0g3519.csv`, test `coremark` (ticlang-bare-o3) |
| CoreMark, -O3, 32 MHz | 78.84 per second (2.464 per MHz) | Same method | measured | `data/results/mspm0g3519.csv`, test `coremark` (ticlang-bare-o3-32mhz) |
| Handwriting model, 64-channel CNN | 2.64M cycles | 32 MHz, same cycles as the LP-MSPM0G3507 | measured | `data/results/mspm0g3519.csv`, test `ai_mnist_cycles` (cnn64-ticlang-o3-32mhz) |
| Interrupt entry | 31 cycles at -O3 and -O2, 34 at -Os | Pended interrupt to the handler's first line | measured | `data/results/mspm0g3519.csv`, test `irq_latency` |
| ADC, 1k samples with DMA | 3.93 Msps (8.3k cycles at 32 MHz) | The G3507 took 8.3k | measured | `data/results/mspm0g3519.csv`, test `adc_max_rate` |
| UART0 loopback, PA10 to PA11 | Clean to 10.67 Mbaud except at 5.5 and 6.0 Mbaud; 12 Mbaud sends nothing | Bytes compared | measured | `data/results/mspm0g3519.csv`, test `uart_max_clean` |
| Zephyr CoreMark, -O2 with unrolling | 135.26 per second | Zephyr 4.4.2 upstream board, GCC 12.2, 80 MHz | measured | `data/results/mspm0g3519.csv`, test `coremark` (gcc-zephyr-o2unroll) |

## If you see this

- If you see `Unidentified PART[0x1508]/variant[0x16], unknown DeviceID[0xbba9]. Attempting to proceed as mspm0x.` from OpenOCD, ignore it: the part is not in xPack OpenOCD 0.12.0-7's table and it still programs and verifies the board.
- If you see `A firmware update is required for the XDS110 probe` (error -264) from UniFlash's command line, the probe shipped on firmware 3.0.0.31. Update it to 3.0.0.43 with the two `xdsdfu` commands in the FAQ.
- If you see `Unknown Device ID[0x0], cannot identify target` and `auto_probe failed`, load any program with UniFlash (`dslite.sh --mode flash ...`); OpenOCD can then read and program again.

The rest, with causes, are in [FAQ.md](FAQ.md).

## Limits

The Zephyr blinky builds and flashes but does not run; the cause is not found (`FAQ.md`).

## Where these facts come from

Checked 2026-10-08 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, UniFlash 9.6.0 and Zephyr 4.4.2, on one LP-MSPM0G3519 (probe firmware 3.0.0.43 after an update; 3.3 V supply read 3.30 V). Data sheet SLASFA2C and user guide SLAU947 read 2026-10-07; TI.com price read 2026-10-04.

Files: `data/results/mspm0g3519.csv`, `RESULTS-mspm0g3519.md`, `AGENTS.md`, `FAQ.md`.
