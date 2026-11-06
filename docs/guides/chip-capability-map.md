# Which TI LaunchPad kit can run a given port? What eight MSPM0, MSPM33, AM13 and CC35X1 boards did (checked 2026-10-21)

Terms: [docs/vocab.md](../vocab.md). Lessons by kind of change: [porting-lessons.md](porting-lessons.md). 1 kB = 1,024 bytes.

RAM decided most outcomes. Doom needed the LP-MSPM33C321A or the LP-EM-CC35X1 for RAM and the 128 kB and 32 kB parts could not hold it. llama2.c needed more flash than the LP-MSPM0G3519's 512 kB. The Game Boy links on the G3519 but only ran on the M33C321A. The first table lists which project ran on which chip and the resource that decided it. Below it, one table per chip lists the peripherals and features the ports used, with the number each produced.

| Chip | Clock | Flash | RAM |
|---|---|---|---|
| LP-MSPM0C1104 | 24 MHz | 16 kB | 1 kB |
| LP-MSPM0L1306 | 32 MHz | 64 kB | 4 kB |
| LP-MSPM0G3507 | 80 MHz | 128 kB | 32 kB |
| LP-MSPM0G3519 | 80 MHz | 512 kB | 128 kB |
| LP-MSPM0G5187 (with NPU) | 80 MHz | 128 kB | 32 kB |
| LP-MSPM33C321A | 160 MHz Cortex-M33 | 1 MB | 256 kB |
| LP-AM13E230 | 200 MHz Cortex-M33 | 512 kB | 128 kB |
| LP-EM-CC35X1 | 160 MHz Cortex-M33 | 8 MB external | 1,126 kB (data/boards.csv) |

Short names in the table: G3519 is LP-MSPM0G3519, M33C321A is LP-MSPM33C321A, AM13E230 is LP-AM13E230, CC35X1 is LP-EM-CC35X1. Tested with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS. TCM is fast tightly coupled RAM, XIP is execute in place, NPU is the neural processing unit, TMU is the trigonometric math unit, HSADC is the high-speed ADC, DWT is the Cortex-M cycle counter, p95 is the value 95 percent of queries stay under, STANDBY0 and STANDBY1 are the chip's low-power states (see vocab), DSLite is TI's command-line flash tool. Sizes are from [data/boards.csv](../../data/boards.csv).

## Project against chip

Works: it ran and did what it should; a note names any part that did not run. Should work: it builds, and only the named run is missing. Could work: porting effort stays, named beside it. Not for this chip: the named resource rules it out. A dash means no run is recorded.

| Project | C1104 | L1306 | G3507 | G3519 | G5187 | M33C321A | AM13E230 | CC35X1 |
|---|---|---|---|---|---|---|---|---|
| [DNS blocker](../../ports/dns-sinkhole/README.md) | - | - | - | - | - | - | - | Works (6 ms median blocked) |
| [Doom](../../ports/doom/README.md) | - | - | Not for this chip (RAM) | Not for this chip (RAM) | - | Works (21.9 frames/s stepped clock, display stubbed; 22.7 real time with the EDUMKII screen; page cache) | - | Works (39.2 frames/s) |
| [MicroPython](../../ports/micropython/README.md) | - | - | Should work (minimal build, not run on the board; full is 45.9 kB over flash) | Works (full, 175.6 kB) | - | - | - | - |
| [Rust](../../ports/rust-mspm0/README.md) | - | Works | - | - | - | - | - | - |
| [wasm3](../../ports/wasm3/README.md) | - | - | Should work (tiny build, 11.1 kB RAM, checked in an emulator; not run on the board) | Works (fib 43.5 times slower than C) | - | Should work (built and emulator-checked; not run on the board) | - | - |
| [llama2.c](../../ports/llama2c/README.md) | - | - | - | Not for this chip (flash: image 543.6 kB) | - | Works (21.2 tokens/s) | - | - |
| [Arduboy games](../../ports/arduboy/README.md) | - | - | Should work (built; not run on the board) | Should work (built; not run on the board) | Should work (built; not run on the board) | Works (about 31 frames/s) | - | - |
| [Game Boy](../../ports/peanut-gb/README.md) | - | - | Not for this chip (flash, RAM) | Should work (links with a null display; not run on the board) | Not for this chip (flash, RAM) | Works (59.7 frames/s) | - | - |
| [2048, Minesweeper](../../ports/touch-games/README.md) | - | - | Should work (built; the display needs wiring to this board's own pins) | Works (drawn and timed; finger touch not recorded) | - | Should work (built with a stub screen; this board has no touch display) | - | - |
| [CHIP-8](../../ports/chip8/README.md) | - | - | - | Works | - | - | - | - |
| [ZX Spectrum](../../ports/zx-spectrum/README.md) | - | - | - | Works (runs at about 19 percent of real speed; the CPU is the limit) | - | - | - | - |
| [MP3 decode](../../ports/minimp3/README.md) | - | - | - | Works (bit-exact; 9.8 times the audio length for stereo, so not real time; no FPU) | - | Works (0.151 times real time) | - | - |
| [uLisp](../../ports/ulisp/README.md) | - | - | Should work (built; not flashed) | Should work (built; not flashed) | Works | - | - | - |
| [MSP430 FRAM examples](../../ports/msp430-to-mspm0/README.md) | - | Works | - | - | - | - | Could work (STANDBY0 never woke; the cause is not confirmed) | - |
| [NPU filters](../../mspm0g5187/npu-dsp/README.md) | - | - | - | - | Works (29.7 times the CPU build) | - | - | - |
| | - | - | - | Works (6 of 6 checks) | - | - | - | - |
| [EDUMKII pack](../../portability/boosterpacks/edumkii/README.md) | - | Works, subset (no screen: debug pins) | Should work (built; jumpers J8, J9, J16, J17, J18 and J22 change) | Should work (built; S1 and the console share PA10 and PA11 with the probe serial line) | Should work (built; the J2 and J4 headers need fitting) | Works | - | Not for this chip (pins) |
| [SHARP128 pack](../../portability/boosterpacks/sharp128/README.md) | Works (609 B RAM) | Should work (built, screen only; no card) | Should work (built; pull jumper J9) | Should work (built) | - | Should work (built; jumper J3 off) | - | Not for this chip (pins) |
| [AUDIO pack](../../portability/boosterpacks/audio/README.md) | - | Not for this chip (pins) | - | Should work (built) | - | Should work (built) | Works | - |
| [BASSENSORSMKII pack](../../portability/boosterpacks/bassensorsmkii/README.md) | - | - | - | Should work (built) | Works, subset (3 of 6 sensors) | Should work (built; the DRV5055 needs the HSADC ported) | - | Not for this chip (pins) |

Sources for the pack rows: [PINS.md](../../portability/boosterpacks/PINS.md). The deciding resource for most "Not for this chip" cells is listed with the failing number in [porting-lessons.md](porting-lessons.md#what-did-not-work-and-why).

## What each chip did

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM0C1104</strong> (16 kB flash, 1 kB RAM): 1 kB RAM, SPI bands, no SysTick</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| SPI (CPU-driven) | Sharp 128 x 128 panel at 1.000 MHz, no frame buffer | 609 B RAM, 4.9 kB flash; whole picture 153.7 ms (1-row bands), 41.1 ms (16-row bands) | The bus runs at 84 percent of the clock rate because of CPU gaps between bytes |
| Timer | TIMG14 counts milliseconds | Hello loop ran 23 counts in 6 s | SysTick does nothing on this chip |
| RAM and stack | Painted stack | 352 of 512 B used | The generated linker file's 64 B default overflows |
| Flash | Sharp driver | 4.9 kB of 16 kB | - |

The microSD video path is built and simulated, not run (no card fitted).

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM0L1306</strong> (64 kB flash, 4 kB RAM): GPIO, UART, ADC, flash write, STANDBY0</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| GPIO, timer | Rust blinky with an async timer | 2.6 kB flash, 112 B RAM | LED1 is active low |
| UART | Rust serial hello (the HAL is a git dependency; the crates.io entry is empty) | 3.7 kB flash, 148 B RAM | RTT output is invisible on the repository's flash path |
| ADC | EDUMKII light sensor, joystick Y, accelerometer X and Y, microphone | Ran with the pack stacked | Screen, S1 and S2 are not reachable: pins 13 and 17 are SWCLK and SWDIO, pins 31 to 33 have no pin; no accelerometer Z (op-amp input) |
| Flash write | Persistent counter as an append log in one erase sector | Word write 56 microseconds, 1 kB sector erase 1.99 ms; count survived 17 resets, a program load and a 5 s power-off | 100k erases in the lower 32 kB; a counter bumped every second lasts about 148 days in one sector |
| Low power | CPU clock in STANDBY0 | 33.8 thousand SysTick ticks a second against 15.2 million in SLEEP; watchdog wake period 0.997 s over 29 ticks | The debugger's power request holds the chip out of STANDBY until cleared |
| UART in STANDBY | Bus-clock UART against 32 kHz UART | 190 of 190 bytes against 5 of 38 | The 32 kHz UART receives in STANDBY1 but loses bursts |

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM0G3507</strong> (128 kB flash, 32 kB RAM): what the builds show</summary>

Build results for this chip:

| Feature | Build result | Limit |
|---|---|---|
| Flash 128 kB | MicroPython minimal 72.9 kB (57 percent); wasm3 55.4 kB | MicroPython full 173.9 kB is 45.9 kB over |
| RAM 32 kB | MicroPython heap 29.1 kB; wasm3 tiny build 11.1 kB total; touch games 5.3 kB | The 256 kB Game Boy ROM does not link; 32 kB frame buffers do not fit |
| Pins | EDUMKII needs jumpers J8, J9, J16, J17, J18 and J22 changed | Header pin 6 is the thermistor through J9 |

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM0G3519</strong> (512 kB flash, 128 kB RAM): SPI at 40 MHz, software I2C, SRAM banks, no FPU</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| SPI (polled) | 3.5 inch panel at 40 MHz | Full-screen fill 67.2 ms, the same as with DMA; a 2048 swipe 32.6 ms mean ([touch games](../../ports/touch-games/README.md)) | 40 MHz is above the panel's rated 15 MHz |
| SPI1 (hardware, from Python) | `machine.SPI` drove the panel | 20 MHz | Fixed pins; SPI1 input clock is MCLK (80 MHz), not the 40 MHz bus clock; a chip-select pulse under about 10 microseconds is lost |
| I2C | Touch chip read by bit-banged GPIO | 0.89 ms for 5 bytes; 16 reads, 0 failures | The chip's I2C block got no answer from the touch chip on these boards (cause not found; suspected setup) |
| UART | MicroPython console, PA10 and PA11 | 115200 baud | Shared with the probe's serial line |
| SRAM banks | ZX Spectrum: 48 kB RAM in bank 0, Z80 core copied to bank 1 | about 16 to 19 percent of a real Spectrum | Bank 0 (64 kB) is full |
| CPU, no FPU | MP3 decode compiled soft float | 25 times the M33's cycles a frame; 7.7 to 10.7 times real time (modelled) | Every float operation is a library call |
| Flash 512 kB | MicroPython full 175.6 kB; UART bootloader with application hand-off | 6 of 6 host checks | A raw binary padded to 16 bytes flashes; the ELF stopped at 91 percent |
| RAM 128 kB | Doom static data alone | Does not fit | MicroPython full leaves a 122 kB heap |

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM0G5187</strong> (128 kB flash, 32 kB RAM): NPU, I2C sensors, UART named UC0</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| NPU | 64-tap FIR, 8-band filter bank, max-pool and 1x1 comparator, 64-point DFT as fixed-weight layers | FIR 0.566 ms against 16.824 ms for TI's CPU build (29.7 times) and 3.286 ms for one hand-written channel (5.8 times); 0 differing bytes in 16 images | Output channels in multiples of 4; 8-bit weights give about 35 dB end to end; DFT reaches 0.26 multiply-adds a cycle (4.3 times) |
| I2C | HDC2080 and OPT3001 on the BASSENSORSMKII pack | 22.0 C, 44.7 percent RH, about 4000 lux | The TMP117 and BMI160 gave no answer (cause not found; suspected: unfitted headers); J2 and J4 headers were not fitted on the unit |
| ADC | DRV5055 magnet sensor | 1.638 V, no magnet | - |
| UART | uLisp REPL over the back channel | 19 of 19 checks; the block is named `UC0`, not `UART0` | A handler with the old name builds and never runs |
| GPIO | Three LEDs | PA17, PB13, PA24 | Different pins from the G3507 and G3519 |
| Flash and RAM | uLisp | 98.8 kB of 128 kB flash, 23.1 kB RAM, 8.9 kB stack left; `(tak 18 12 6)` 7.5 s at 80 MHz | The 256 kB Game Boy ROM does not link |

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-MSPM33C321A</strong> (1 MB flash, 256 kB RAM): FPU, QSPI, SPI with DMA, 160 MHz</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| FPU | minimp3 (1.9k FPU instructions, no float library calls) | 0.151 times real time stereo, 122.35M cycles; PCM equal to the Mac, 194 of 194 frames | Needs 24.1 MHz to keep up in real time |
| Cycle counter (DWT) | Timed each decode call and each Doom frame | minimp3 cycle counts identical on two runs | 32-bit; wraps in 26.8 s at 160 MHz |
| QSPI | Dump, guarded write and page-cache reads of a 16 MB NOR | Dump equal twice; 7,033 page writes with verify_bad 0; a 256-byte polled quad read is 23.8 microseconds | No memory-mapped window in SDK 1.04.00.00; the unit holds 16 MB (JEDEC ID C2 20 18), not the 8 MB the board guide lists |
| SPI with DMA | EDUMKII panel; Game Boy and Doom rows | Panel frame 19.5 ms at 16 MHz; Doom 80 rows in 14.6 ms in the background | Do not flush the SPI FIFO between rows |
| ADC (HSADC) | Joystick through the sequencer | Scripted run: 79 key events | Stick press tied to ground (PA21 is VREF-; board wiring); the DRV5055 read returns -1 because the HSADC is not ported in that driver |
| Clock | PLL | 160 MHz after `PLL_QDIV` 10 and `EXTDIVMCLK` 1 | Default tree gives 32 MHz |
| Flash 1 MB, RAM 256 kB | llama2.c 543.6 kB flash; Doom with 30 kB zone | 21.2 tokens/s; Doom 21.9 frames/s at 16 kB cache | Stock Doom needs 164 kB static and an 800 kB zone |

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-AM13E230</strong> (512 kB flash, 128 kB RAM): GPIO-driven DAC, ADC, SLEEP, flash tools</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| GPIO | DAC8311 driven as plain GPIO at 8 kHz samples from SysTick | Pack's microphone heard 1000.0 Hz, 99.9 percent of energy within 50 Hz | One 16-bit frame per sample; the amplifier line must be low |
| ADC | Microphone capture, 2k samples at 8 kHz | Floor 25 to 34 mV RMS | - |
| SLEEP and watchdog | Interval interrupt in SLEEP | 5 ticks at 1.0 s | - |
| STANDBY0 | Watchdog interval as the only wake source | Never woke; every connect failed (DSLite -613, -6310, -6311). The image used a wake source the SDK does not list for STANDBY, so the cause is not confirmed | The SDK's own example wakes STANDBY from a GPIO |
| Flash tools | DSLite erase scopes | "Necessary Bank(s) Only", "Entire Flash"; `-e` means verbose | Program data in the bank the program does not use |

The TMU and NPU were not used by any port. Flash write and the persistent-counter examples are built, not run.

</details>

<details markdown="1">
<summary markdown="span"><strong>LP-EM-CC35X1</strong> (8 MB external flash, 1,126 kB RAM): Wi-Fi, memory-mapped flash, FreeRTOS</summary>

| Feature | What a port did | Number | Limit |
|---|---|---|---|
| Wi-Fi sockets (lwIP, UDP) | DNS blocker on port 53, 71,492-domain table | Blocked median 6 ms (p95 20), forwarded 70 ms (p95 172) against 52 ms asking the gateway directly; 1,034 queries, no drops | Raw UDP callbacks run on the tcpip thread; no blocking calls inside |
| External flash, execute in place | Doom data file read through the mapped address | 4,097.7 kB read in place; 44 cycles a word, no data cache | Over-the-air layout leaves a 2,688 kB slot; switch it off for 6,248 kB |
| Memory regions | Doom placement | Load cost: TCM (128 kB) 1.06 cycles, code RAM (32 kB) 1.6, DRAM (512 kB) 6.2 | 22.5 frames/s before placement, 36.7 after, 39.2 at -O2 |
| UART | Console | 921,600 baud for Doom, 115200 for SDK examples | Silent after a changed flash layout until `full_flash_erase` |
| Cycle counter | Profiling | `CYCCNT` reads 0 | Use the RTOS tick plus SysTick |
| BoosterPack header | EDUMKII, SHARP128, BASSENSORSMKII | - | Eight pins (SHARP128: three) cannot be freed with a jumper |

</details>

## Questions

**Which chip ran the most projects?** Counting Works in the first table, the LP-MSPM0G3519 ran MicroPython, wasm3, 2048 and Minesweeper, CHIP-8, a ZX Spectrum, a UART bootloader and MP3 decode. The LP-MSPM33C321A ran Doom, a Game Boy, llama2.c and MP3 decoding, which need its RAM, FPU and 160 MHz.

**Will Doom run on an LP-MSPM0G3519?** No. Doom's static data alone does not fit in its 128 kB of RAM (the MicroPython full build leaves a 122 kB heap there). It ran on the LP-MSPM33C321A and the LP-EM-CC35X1; see [the Doom write-up](../../ports/doom/WRITEUP.md).

**Why does the EDUMKII screen not work on the LP-MSPM0L1306?** Header pins 13 and 17 are the debug pins SWCLK and SWDIO, so the screen, S1 and S2 cannot be reached. The analog parts of the pack ran.

**Why did the LP-AM13E230 never wake from STANDBY0?** Unknown. A watchdog interval was the only wake source and every connect after it failed with DSLite -613, -6310 or -6311. The SDK names a GPIO as the STANDBY wake source and the watchdog for STOP; no image with a GPIO wake was run, so whether the wake source was the cause is not confirmed.

**Does the LP-MSPM0G3519 have a floating-point unit?** No. MP3 decode there takes 25 times the M33C321A's cycles a frame.

**Why is my LP-EM-CC35X1 console silent after I change the flash layout?** It stays silent until `full_flash_erase`. Check the baud too: 921,600 for Doom, 115200 for SDK examples.

**Which chip do I buy for an NPU demo?** The LP-MSPM0G5187: its NPU ran a 64-tap FIR 29.7 times faster than the CPU build.

**Does the LP-EM-CC35X1 take BoosterPacks?** Not the EDUMKII, SHARP128 or BASSENSORSMKII: eight pins (three for SHARP128) cannot be freed with a jumper, a board layout limit.

**Which chip should a first port use?** The LP-MSPM0G3519 ran the widest range; take the LP-MSPM33C321A (1 MB, 256 kB, FPU) for decoders, language models and emulators. For a first port, open the [MicroPython README](../../ports/micropython/README.md).
