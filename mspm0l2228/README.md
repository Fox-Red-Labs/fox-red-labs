# Does the LP-MSPM0L2228 LaunchPad build, flash and run from a Mac terminal?

Yes. TI's blink builds in 2.5 to 3.6 s and flashes in 4.2 to 4.8 s with DSLite, and CoreMark at 32 MHz scores 74.58 per second at -O3 (2.331 per MHz). Not run yet: power, the AES, random-number and ADC tests, and the AI cost runs. TI's LCD example wrote LCD memory; the glass was not looked at. Every number is a row in `data/results/mspm0l2228.csv`.

**First blink:** `./start.sh mspm0l2228 blink` from the top of a clone, once the tools are installed ([One-command build](#one-command-build)). It backs the board up, builds TI's blink, flashes it and times the LED pin; one run took 41 s including the backup.

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. **CoreMark** is a standard CPU speed test; **DSLite** is TI's command-line flasher; **SysConfig** is TI's pin and clock setup tool; **NONMAIN** is the flash area that holds the chip's boot settings.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `WALKTHROUGH.md`; `STOPWATCH.md` and `TIMELINE.md` (the blink clocks); `bench/` (build and run scripts for CoreMark and the timing test, plus the DSLite helpers); `blink/` and `lcd/` (copies of two TI examples); `sweep/builds.jsonl` (the build pass); `data/` (the board's shipped flash image and a read-only copy of NONMAIN).

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; versions are under "Where these facts come from"); built images (`.out`, `.bin`), which are not cleared for redistribution, so you build them with the commands below. The numbers are rows in `data/results/mspm0l2228.csv` at the top of the repository.

**Taking this folder alone:** the commands run from the top of a clone and call `tools/` and `data/`; the benchmarks borrow sources from `third-party/coremark`, `mspm0l1306/bench/coremark` and `mspm0g3519/bench/common`.

## The board

The LP-MSPM0L2228 LaunchPad development kit carries an MSPM0L2228SPN in an 80-pin package. Sources: data sheet SLASF94A and user guide SLAU928B, read 2026-11-09; the identity words were read from the chip.

| Part | Fact | Read from |
|---|---|---|
| Chip | MSPM0L2228: Arm Cortex-M0+ at 32 MHz | data sheet SLASF94A; `data/results/mspm0l2228.csv`, test `connect_step` (DEVICEID 0x1BB9F02F) |
| Memory | 256 kB dual-bank flash, 32 kB RAM | SRAMFLASH register 0x00201100 on the chip |
| On the chip | Segment LCD controller, AES, a true random-number generator, 12-bit ADC, a battery-backed domain (RTC, tamper pins, 32 bytes of backup memory). No floating point, no CAN, no USB | data sheet SLASF94A |
| Debugger | XDS110 with EnergyTrace; it shipped on probe firmware 3.0.0.28, which DSLite refuses (see [FAQ](FAQ.md)) | user guide SLAU928B; `FAQ.md` |
| LEDs and buttons | RGB LED: red PB10, green PB9, blue PA16 on this Rev. A board by its silkscreen (TI's user guide and example say PA23, the Rev E board). Red LED on PA0. Buttons S1 (PA18, BSL), S2 (PB8), reset S3 | user guide SLAU928B; `mspm0l2228/blink/gpio_toggle_output/gpio_toggle_output.syscfg`; silkscreen of the board; E2E thread 1497347 |
| Display | A 7-segment LCD glass on the board | user guide SLAU928B |
| Serial | UART0 on PA10 (TX) and PA11 (RX) through jumpers J17 and J18 on XDS; port `/dev/cu.usbmodemML2200011` was silent for 5 s after plug-in | `data/results/mspm0l2228.csv`, test `serial_idle` |

## Ten-minute path (measured on an Intel Mac)

You need the board, a micro-USB cable, and a Mac with CCStudio 21.0.1 (it carries DSLite and the compiler) and the MSPM0 SDK 2.11.00.07. Everything happens in a terminal.

1. Check the probe firmware. If it is below 3.0.0.43 DSLite refuses to connect (the [FAQ](FAQ.md) has the fix).

```bash
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
```

2. Copy TI's blink example and build it. It took 3.6 s the first time and 2.5 s the second.

```bash
cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0L2228/driverlib/gpio_toggle_output .
```

```bash
../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
```

3. Flash it with DSLite and a connection file for your probe serial. `MSPM0L2228_ML220001.ccxml` is this repo's file; make yours with the command below. It took 4.8 s, then 4.2 s. `mspm0l2228/bench/dsflash.sh <image.out>` wraps the command with a retry loop.

```bash
sed 's/ML220001/<your serial>/g' mspm0l2228/MSPM0L2228_ML220001.ccxml > MSPM0L2228_<your serial>.ccxml
```

```bash
/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash -c MSPM0L2228_ML220001.ccxml -e -f -v gpio_toggle_output/ticlang/gpio_toggle_output.out
```

4. See the LEDs change. TI's example toggles PA23, PB10, PB9 and PA21; on this Rev. A board the blue RGB LED is wired to PA16, so it may stay dark (see the [FAQ](FAQ.md)). The red LED PB10 is the one the one-command run timed. Read the output registers with `mspm0l2228/bench/pinread.sh 5`; the values changed between reads, which shows the outputs change and not which LED you see. Then change `DELAY` in `gpio_toggle_output.c` (16M cycles is about 0.5 s by the clock math; not timed), rebuild and flash again.

xPack OpenOCD 0.12.0-7 connects to this chip, reads its identity (`0x41c40004: 1bb9f02f 803c2c38`) and flashes it (20.6 s for the blink image, against DSLite's 4.2 s), although its part table does not list the MSPM0L2228. After the restore of the shipped image, the pin that toggles is PA0 (the red LED LED3), not the green RGB pin TI's README names; the cause was not found.

## One-command build

```bash
./start.sh mspm0l2228 blink
```

From the top of a clone. It finds the probe, backs up main flash and NONMAIN (read twice, compared), copies TI's `gpio_toggle_output`, sets the LED pattern, builds it (3.0 s), writes it over OpenOCD and reads it back (20.6 s), then times the red LED pin to the asked pattern. One run: 41 s with the backup. `--pattern 150,150,150,900` sets the on and off times in milliseconds. It never writes NONMAIN. The run prints the backup folder. Put the board back with `python3 tools/frl/frl.py restore <backup folder> --probe <serial>`.

The benchmarks build on their own: `mspm0l2228/bench/coremark/build.sh` builds CoreMark at -O3, -O2, -Os and -Oz for 32 MHz (about a minute); `bench/timing/build.sh O3` builds the GPIO and interrupt timing test; `python3 tools/board_sweep/build_all.py` builds every SDK example (see below). None of those flashes or runs anything.

## What was measured

2026-11-09, one board, code run from flash, probe firmware 3.0.0.43.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Chip: 32 MHz, 256 kB flash, 32 kB RAM | as listed | Vendor figures | claim | data sheet SLASF94A |
| Build of TI's blink | 2.5 s (3.6 s the first time) | `sdkmake.sh`, 2 runs | measured | `data/results/mspm0l2228.csv`, test `blink_build` |
| Flash with DSLite | 4.2 s (4.8 s the first time) | erase and verify, 520-byte image, 2 runs | measured | `data/results/mspm0l2228.csv`, test `blink_flash` |
| Blink from build to flashed | 8.4 s | by hand, one run; pins read to show they change | measured | `data/results/mspm0l2228.csv`, test `time_to_blink` |
| One command to a timed blink | 41 s | the one-command script, one run, backup included; build 3.0 s, OpenOCD write and read-back 20.6 s; red LED (PB10) timed to 150, 150, 150, 900 ms | measured | `data/results/mspm0l2228.csv`, tests `time_to_blink` (frl), `blink_flash` (frl-openocd) |
| CoreMark, -O3, 32 MHz | 74.58 per second (2.331 per MHz) | Three identical runs, validated by CRC | measured | `data/results/mspm0l2228.csv`, test `coremark` (ticlang-bare-o3-32mhz) |
| CoreMark, -O2, -Os, -Oz | 51.94, 47.40, 42.54 per second | Same method | measured | `data/results/mspm0l2228.csv`, test `coremark` |
| Print a message | Changed welcome line "Hello from Fox Red Labs" read once at 115200, then "> Testing...0" to "9" each second; flash 4.5 s | TI's UART console example, 2 runs | measured | `data/results/mspm0l2228.csv`, test `message_flash` |
| Four room-occupancy models | 9.4, 21.4, 81.6, 294.5 ms; 20 of 20 vectors right each | plain C, -O3, 32 MHz, fastest of 20 vectors | measured | `data/results/mspm0l2228.csv`, tests `ai_occ_latency`, `ai_occ_fit` |
| BitNetMCU 1k and 12k | 94 k and 443 k cycles (94,251 and 443,117); 10 of 10 digits right | -O3, 32 MHz, mean of 10 digits | measured | `data/results/mspm0l2228.csv`, test `ai_mnist_cycles` |
| Supply voltage through the ADC | 3.307 to 3.309 V | TI's adc12_monitor_supply, 3 flashes; no outside meter | measured | `data/results/mspm0l2228.csv`, test `port_example_behavior` |
| Timer PWM (dead band) | counter runs; compare values 1000 and 499 of 2000; PA0 muxed to the timer | TI's tima_timer_mode_pwm_dead_band, registers read; pins not measured | measured | `data/results/mspm0l2228.csv`, test `port_example_behavior` |
| GPIO store | 1.0 cycle (1.18 at -Os) | 16 back-to-back stores minus a SysTick pair | measured | `data/results/mspm0l2228.csv`, test `gpio_toggle` (register) |
| Driver toggle | 1.05 cycles at -O3 and -O2, 8.0 at -Os | DL_GPIO_togglePins in a loop | measured | `data/results/mspm0l2228.csv`, test `gpio_toggle` (driver) |
| Interrupt entry | 33 cycles at -O2 and -Os, 37 at -O3 | Pended interrupt to the handler's first line | measured | `data/results/mspm0l2228.csv`, test `irq_latency` |
| LCD example | Wrote LCD memory; the glass was not looked at | TI's `lcd_write_memory`; LCD control register 0x0000181D, memory word changed between reads | measured (self-report) | `data/results/mspm0l2228.csv`, test `port_example_behavior` |
| SDK examples that build | 96.6 percent (172 of 178) | One make per example; not run | measured | `data/results/mspm0l2228.csv`, test `examples_built` |
| Restore of the shipped image | Identical on read-back | 256 kB main flash, 17 s; NONMAIN unchanged | measured | `data/results/mspm0l2228.csv`, test `backup_restore` |

CoreMark at -O3 is 5 percent below the LP-MSPM0G3519 at the same 32 MHz (78.84), although both read two flash wait states; the cause was not found.

## If you see this

- If you see `A firmware update is required for the XDS110 probe` from DSLite, the probe shipped on firmware 3.0.0.28. Update it to 3.0.0.43 with `xdsdfu` (the [FAQ](FAQ.md) has the steps).
- If you see `.../sysconfig_1.26.3/sysconfig_cli.sh: No such file or directory`, SysConfig 1.26.3 is not installed; use the version you have (`tools/frl/toolenv.py mspm0` prints it).

The rest, with causes, are in [FAQ.md](FAQ.md).

## Limits

Not run: the example sweep (built only), CRC and AES against software, random-number rate, ADC rate, the LCD on a camera, power and EnergyTrace, Zephyr, and the AI cost runs.

## Where these facts come from

Checked 2026-11-09 on an Intel Mac with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, CCStudio 21.0.1 (DSLite) and xPack OpenOCD 0.12.0-7, on one LP-MSPM0L2228 (probe ML220001, firmware 3.0.0.43 after an update). Data sheet SLASF94A and user guide SLAU928B read the same day.

Files: `data/results/mspm0l2228.csv`, `STOPWATCH.md`, `TIMELINE.md`, `AGENTS.md`, `FAQ.md`.
