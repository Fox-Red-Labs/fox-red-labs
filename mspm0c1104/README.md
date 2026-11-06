# TI LP-MSPM0C1104: from box to blinking LED on a Mac, and what fits in 1 kB of RAM

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Short answer.** With the tools installed, a copied TI example went from first command to a changed blink in 77 seconds on 2026-10-08 (build 3.5 s, flash 0.64 s). Of the 19 Embench programs only 9 link in 1 kB of RAM, standard CoreMark and 256-point FFT do not fit, and Dhrystone does not fit either. The CCStudio route has not been timed.

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes. **Embench** is a set of 19 small test programs; **SysTick** is the Arm core's built-in timer; **link only** means the linker accepted or rejected the program, nothing was run; **NONMAIN** is the flash area that holds the chip's boot settings.

## What is in this folder, and what is not

**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); and `freertos/`, the FreeRTOS build. Results are rows in `data/results/mspm0c1104.csv` at the top of the repository.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"), and built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add mspm0c1104 tools data mspm0g3507/mcu-bench third-party/coremark
```

`docs/` is left out on purpose (it is mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.

## The board

The LP-MSPM0C1104 is TI's smallest MSPM0 LaunchPad development kit, on a 20-pin header with female sockets.

| Part | Fact | Read from |
|---|---|---|
| Chip | MSPM0C1104: Arm Cortex-M0+ at 24 MHz, no PLL, no floating point | data sheet SLASF90E, read 2026-10-07 |
| Memory | 16 kB flash, 1 kB RAM | data sheet SLASF90E |
| Debugger | Onboard XDS110, probe MC110001, shipped on firmware 3.0.0.26 | `data/results/mspm0c1104.csv`, test `probe_included` |
| LED and button | One red LED (PA22 on board version E3 and later, PA4 on E2; jumper J7 must be on) and one user button | user guide SLAU908B, Table 2-1; `FAQ.md` |
| Serial | UART0 PA27 (TX) and PA26 (RX), through shunts on header J101 rows 3 and 4 | `data/results/mspm0c1104.csv` |
| Not on the chip | No DAC, no math accelerator, no AES block; hardware CRC-16 only; SysTick does nothing | `FAQ.md`, `data/results/mspm0c1104.csv` |

## Ten-minute path (terminal route measured)

You need: the board, a micro-USB data cable (none in the box, per SLAU908B section 1.2), an Intel or Apple Silicon Mac with CCStudio 21.0.1 and the MSPM0 SDK 2.11.00.07 installed (`mac-setup/README.md`). With the tools installed, a copied TI example went from first command to a changed blink in 77 seconds on 2026-10-08 (build 3.5 s, flash 0.64 s). The CCStudio route has not been timed.

1. **Plug in the board (bench).** It runs TI's out-of-box demo: the red LED is on (SDK `out_of_box` README).
2. **Copy TI's blink example out of the SDK (terminal).**

   ```bash
   cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0C1104/driverlib/gpio_toggle_output .
   ```
3. **Build it (terminal).**

   ```bash
   ../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
   ```
4. **Flash it (terminal).** xPack OpenOCD 0.12.0-7 lists this chip; replace `<serial>` with your probe's, from `ls /dev/cu.usbmodem*`:

   ```bash
   openocd -f interface/xds110.cfg -c "adapter serial <serial>" -f target/ti_mspm0.cfg -c init -c "reset halt" -c "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" -c "reset run" -c shutdown
   ```
5. **Look at the LED (bench).** The red LED blinks. Change the delay in `gpio_toggle_output.c`, rebuild, flash, and watch the rate change: that is the finish line.

## One-command build

Everything that needs no board (blink, CoreMark, the occupancy and handwriting models, every SDK example's build pass), into `out/mspm0c1104/`:

```bash
./prebuild.sh
```

It waits for a quiet machine and prints install steps if a tool is missing. It does not flash or run anything.

## What was measured

Of the 19 Embench programs only 9 link in 1 kB of RAM, standard CoreMark and 256-point FFT do not fit, and Dhrystone does not fit either. Link-only results are computed: the linker judged them, no program ran.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Changed blink, first command to proof | 77 s | Tools installed; plug-in, identify and backup (about 9 min) not counted; proof by pin read (PA22 toggle 496 ms before, 124 ms after) | measured | `data/results/mspm0c1104.csv`, test `time_to_blink` (cli) |
| Build of TI's blink | 3.5 s (3.5 to 4.3 s over 3 runs) | `sdkmake.sh`, TI Arm Clang 5.1.1.LTS | measured | `data/results/mspm0c1104.csv`, test `blink_build` |
| Flash with OpenOCD | 0.64 s | 424-byte blink, program verify reset, 2500 kHz adapter | measured | `data/results/mspm0c1104.csv`, test `blink_flash` |
| TI's blink example size | 424 B of flash, 64 B of RAM (all stack) | Of 16 kB and 1 kB | measured | `data/results/mspm0c1104.csv`, tests `blink_flash_bytes`, `blink_ram_bytes` |
| Standard CoreMark (2,000-byte data set) | does not fit | The data set alone is twice the RAM; 20.2 kB of code in 16 kB of flash, a 2.5 kB stack in 1 kB of RAM; link only | computed | `data/results/mspm0c1104.csv`, test `coremark` |
| Reduced CoreMark (400 and 200 bytes) | links, then crashes | Hard fault in `core_bench_list` with the stack pointer below RAM; cause not found | measured | `data/results/mspm0c1104.csv`, test `coremark` (ticlang-bare-os-400b) |
| Dhrystone 2.1 | does not fit | One of its arrays is 1.1 kB after the cut that made it fit the LP-MSPM0L1306; arithmetic, not linked | computed | `data/results/mspm0c1104.csv`, test `dhrystone` |
| 256-point q15 FFT | does not fit | 256 complex q15 points need 1 kB alone; arithmetic | computed | `data/results/mspm0c1104.csv`, test `fft256` |
| Embench-IoT | 9 of 19 link; 7 run and pass at -O2, 6 at -Os | The others require 698 B to 8.5 kB more RAM, or more flash; minver and slre run out of stack | measured | `data/results/mspm0c1104.csv`, test `embench_fit` |
| FreeRTOS (V11.2.0), one task plus idle | 4.1 kB flash, 1.0 kB of 1 kB RAM; task switch 478 cycles (19.9 us) | Details in [freertos/README.md](freertos/README.md) | measured | `data/results/mspm0c1104.csv`, tests `blink_flash_bytes`, `blink_ram_bytes`, `rtos_task_switch` (freertos) |
| Room-occupancy model, smallest (636 weights) | does not fit as built | Its buffers need about 1.2 kB; link only | computed | `data/results/mspm0c1104.csv`, test `ai_occ_fit` |
| BitNetMCU 1k handwriting model | 10 of 10 test digits right | 7.2 kB of code, 804 B of RAM; cycles not measured (no working SysTick) | measured | `data/results/mspm0c1104.csv`, test `ai_vendor_example` (bitnetmcu-1k) |
| Zephyr | not supported | 1 kB of RAM, and the L1306's 4 kB was already tight; no board file | claim | `data/results/mspm0c1104.csv`, test `zephyr_support` |
| Interrupt latency | 23 to 25 cycles | Pend I2C0 line 24 to the handler; best and worst of 200 at each of -O3, -O2, -Os | measured | `data/results/mspm0c1104.csv`, test `irq_latency` |
| GPIO store | 2 cycles | Register write, -O3 | measured | `data/results/mspm0c1104.csv`, test `gpio_toggle` (register-bare-o3) |
| ADC with TI's DMA example | 1.0 million samples per second at 10 bits; 0.84 million at 12 bits | Supply 3.32 V | measured | `data/results/mspm0c1104.csv`, test `adc_max_rate` |
| Hardware CRC-16 over 256 bytes | 1.1k cycles, 3.0 times faster than a 256-entry table | Check value 0x29B1 matched | measured | `data/results/mspm0c1104.csv`, test `crc_speedup` (crc16-256b) |
| SDK sweep | 91 of 91 build (90 from their makefile); 83 flashed and run without a crash (68 ordinary, 15 low-power) | One example failed once with no error text and built on retry; 7 that write NONMAIN skipped on purpose | measured | `data/results/mspm0c1104.csv`, tests `examples_built`, `examples_ran_clean` |

Not run: the CCStudio route, power, AI cost runs. Each has a row with its reason in the CSV.

## What fits in 1 kB of RAM

| Program | Result | Why |
|---|---|---|
| TI's blink example | fits | 424 B of flash and 64 B of RAM (all stack), measured |
| Standard CoreMark (2,000-byte data set) | does not fit | the data set alone is twice the RAM; the linker stops on flash and stack |
| Reduced CoreMark (400 and 200 bytes) | links, then crashes | the chip takes a hard fault in `core_bench_list` with the stack pointer below RAM; cause not found |
| Dhrystone 2.1 | does not fit | one of its arrays is 1.1 kB after the cut that made it fit the LP-MSPM0L1306 |
| Embench-IoT | 9 of 19 link, 7 run and pass | the others require 698 B to 8.5 kB more RAM, or more flash; minver and slre run out of stack |
| FreeRTOS (V11.2.0) | one task plus idle fits | 4.1 kB of flash and 1.0 kB of 1 kB RAM; task switch 478 cycles (19.9 us); details in [freertos/README.md](freertos/README.md) |
| Room-occupancy model, smallest (636 weights) | does not fit as built | its buffers need 1.2 kB (link only) |
| BitNetMCU 1k handwriting model | runs, 10 of 10 test digits right | 7.2 kB of code, 804 B of RAM |
| Zephyr | not supported | no board file, and the L1306's 4 kB was already tight |

## If you see this

- If you see `Example requires Rev E3 or later of the MSPM0C1104 LaunchPad` (the SDK README), or a dark LED after a good flash, a version E2 board wires the LED to PA4 and the example toggles PA22. Change the pin in `gpio_toggle_output.syscfg`, or watch J1 pin 8. Jumper J7 must be on.
- If you see `Warning: A firmware update is required for the XDS110 probe. The current firmware is version 3.0.0.26`, do nothing: DSLite still flashes and verifies, and OpenOCD printed nothing. Update only if a tool refuses to connect.
- If you see a hard fault in `core_bench_list` with stack pointer `0x1FFFFF30` (reduced CoreMark with 400 or 200 B of data), the cause is not found and the whole stack is used. Report no CoreMark score for this chip from these builds.

The rest, with causes, are in [FAQ.md](FAQ.md).

## Limits

The reduced-CoreMark build hard-faults; the cause is not found.

## Where these facts come from

Checked 2026-10-08 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0 (0.12.0-7 for the flash command) and UniFlash 9.6.0, on one LP-MSPM0C1104 (probe MC110001, firmware 3.0.0.26). Data sheet SLASF90E and user guide SLAU908B, both read 2026-10-07; TI.com price read 2026-10-04.

Files: `data/results/mspm0c1104.csv`, `AGENTS.md`, `FAQ.md`.
