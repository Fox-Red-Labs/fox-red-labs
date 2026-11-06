# TI LP-MSPM0G3519 speed results, compared with the LP-MSPM0G3507

**Short answer.** At 80 MHz it matches the LP-MSPM0G3507 almost exactly, with four times the memory: CoreMark 182.73 per second at -O3 (2.284 per MHz, against 183.61), the handwriting model in the same 2.64M cycles at 32 MHz, the 30k-weight room model in 129.39 ms (129.40), the control loop at 4.29, 10.79, 37.95 and 70.30 kHz in its four builds (4.26, 10.94, 38.11 and 70.80). It differs in two places: interrupt entry takes 89 clocks (the G3507 took 69), and the UART loopback fails at different speeds.

Terms: [vocab](../docs/vocab.md). Errors and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes.

Measured 2026-10-08 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, UniFlash 9.6.0, probe firmware 3.0.0.43, USB power (3.3 V supply read 3.30 V). Code runs from flash. Every row, with its conditions and a source file, is in `data/results/mspm0g3519.csv`. Not run: power, the AI cost runs, the display's visual check, and tests that need a port not written (listed in the CSV with the reason).

## Setup

| Test | Result | Recipe |
|---|---|---|
| Build TI's blink example | 3.2 s median (4.1, 3.2, 3.0) | `sdkmake.sh`, fresh copy each run |
| Flash it with UniFlash | 4.6 s median (4.8, 4.6, 4.6) | `dslite.sh --mode flash -e -f -v` |
| Flash it with OpenOCD | 0.6 s (the first run after a UniFlash load took 20.6 s) | program, verify, reset; the part is not in OpenOCD's table and it flashes anyway |
| Zephyr blinky build and flash | 51.5 s and 4.5 s median, 11.8 kB flash, 4 kB RAM | does not run: see the [FAQ](FAQ.md) |

## Core, bare metal

CoreMark, three runs each, identical every time, all validated by the CRC.

| Flags | 80 MHz, per second (per MHz) | 32 MHz, per second (per MHz) |
|---|---|---|
| -O3 | 182.73 (2.284) | 78.84 (2.464) |
| -O2 | 122.72 (1.534) | 54.80 (1.713) |
| -Os | 112.87 (1.411) | 49.42 (1.544) |
| -Oz | 100.85 (1.261) | 44.17 (1.380) |

The LP-MSPM0G3507 gave 183.61, 122.58, 114.55 and 102.16 at 80 MHz and 78.82 at -O3 and 32 MHz. The 32 MHz per-MHz figure equals the figure Arm publishes for this core (2.46). The `FLASHWAIT` field reads 2 at both clocks, so it does not show where the extra cost at 80 MHz comes from.

GPIO toggle: 1.0 cycle for a register write, 1.11 cycles per pass through the driver call (8.0 at -Os), as on the G3507. Interrupt entry from a pended interrupt to the handler's first line: 31 cycles at -O3 and -O2, 34 at -Os (the G3507: 29 to 36).

## Core, Zephyr

Zephyr 4.4.2 upstream board, GCC 12.2, 80 MHz, results read from the backchannel serial port (the jumpers J12 and J13 on the XDS side). CoreMark per second: -Os 91.95, -O2 125.99, -O2 with unrolling 135.26, true -O3 130.16 (the G3507: 94.98, 130.23, 139.88, 136.7, about 3% higher). Interrupt entry through Zephyr's wrapper 157 cycles at best (163), a thread yield round trip 559 cycles or 6.99 us (6.717 us), MATHACL sine and cosine 109.8 cycles against 10,242 for the C library, divide 94.0 against 263.3 cycles. The Zephyr blinky itself does not run (see the FAQ).

## Analog and the two loops

| Test | Result |
|---|---|
| ADC, 1k samples with DMA | 8.3k cycles at 32 MHz, 3.93 Msps (G3507 8.3k) |
| DAC to ADC through a wire, PA15 to PA25 | gain 1.00055 (reads 0.055% high), mid-range error within 2.8 codes, noise 1.05 codes; the G3507 read 0.34% low |
| UART0 loopback, PA10 to PA11 | clean from 9,600 baud to 10.67 Mbaud except at 5.5 and 6.0 Mbaud; 12 Mbaud sends nothing |

## Control loop

The same field-oriented control step as the C2000 bench, byte for byte, with a 50 kHz PWM starting each ADC conversion. Highest rate with no missed period in 1,000, median of three runs.

| Build | Highest rate | CPU load at half that rate (percent of a 100 kHz period) |
|---|---|---|
| Software float, -O2 | 4.29 kHz | 1,952 |
| MATHACL sine and cosine, -O2 | 10.79 kHz | 900 |
| IQmath software library | 37.95 kHz | 253 |
| IQmath with MATHACL | 70.30 kHz | 133 |

Interrupt entry (trigger to first line) is 89.0 clocks (1.11 us); the same program on the G3507 measured 68.8. The reason it is 20 clocks longer is not known; it was the same on all three runs.

## Edge AI

All four room-occupancy models give the right answers on all 20 test vectors in every build. Fastest vector, milliseconds at 80 MHz:

| Model | plain C -Os | plain C -O3 | TI NNC, CPU |
|---|---|---|---|
| occ_cnn_500 | 4.67 | 3.91 | 4.93 |
| occ_cnn_2k | 10.87 | 9.04 | 11.98 |
| occ_cnn_8k | 39.93 | 35.27 | 45.65 |
| occ_cnn_30k | 145.35 | 129.39 | 159.13 |

The BitNetMCU handwriting models get 10 of 10 digits on every build, in the same cycles as on the G3507: 2,640,383 (cnn_64, 32 MHz), 2,750,868 (cnn_64, 80 MHz), 660,687, 429,789 and 89,336.

## Touch display

The Waveshare 3.5 inch display answered: the screen's ID read back as the ST7796S pattern (0x77 0x96, shifted by one bit), the touch chip answered at I2C address 0x38 with vendor 0x11 and chip 0x64 (FT6336U) over software I2C, and a full-screen fill took 364.8 ms at 8 MHz SPI (G3507: 364.9). The screen's picture was not checked by eye in this run.

### LVGL on the display (2026-10-09)

The G3507 LVGL 9.6 demo (same trimmed build, 12 kB heap, 6 kB stack, 320 x 10 draw buffer) refreshes **7 times a second at 32 MHz with 16 MHz SPI** and **15 to 19 times a second (mean 17.1) at 80 MHz with 40 MHz SPI**, the same as the G3507 (7, and 15 to 19). Three runs of each gave identical lines, so the demo is repeatable cycle for cycle. The numbers came over the backchannel UART and are firmware counters; the demo was seen on the screen on 2026-10-09 in both runs (7 and 15 to 19 a second), and the screen was not looked at for the 20 MHz SPI and 40-row-buffer variants. A refresh redraws only the changed parts of the screen (about 25,000 pixels at 32 MHz), not the whole 153,600-pixel screen. Flash use is 125.5 kB (32 MHz) and 126.2 kB (80 MHz) of 512 kB; RAM 25.1 kB and 25.2 kB of 128 kB.

What the extra room bought (one run each): a 40-row draw buffer (25.6 kB) takes the 32 MHz build from 7 to 11.8 on average, because LVGL sends fewer, larger strips; at 80 MHz it stays at 17 with the CPU only 43% busy, so the rate there is set by something other than drawing (cause not found). The full demo with the second font that the G3507 had to cut (146.8 kB) runs at 7 fps at 32 MHz, the same as the cut one. 80 MHz with 20 MHz SPI gives 13 to 18 (mean 15.6). Build and flash steps are in `mspm0g3507/lvgl-g3507/README.md`.

## Third-party code, DSP and the example sweep (2026-10-09)

Third-party code, three runs each, identical every time (the true random-number test differs by design). Embench-IoT: 18 of 19 pass at -O2 and -Os, `st` fails its own check as on the G3507, and 18 of 19 land within 1 percent of the G3507's cycles (`crc32` is 9 percent lower at -O2 and 14 percent lower at -Os; cause not tested). AES-128 on 1 kB: the hardware AESADV takes 9.6k cycles for CBC encrypt against 21,416 on the G3507's AES, which makes it 45.8 times faster than tiny-AES-c (the G3507: 20.3), because AESADV chains CBC itself. CRC-32 on 1 kB: hardware 1.9k cycles with 32-bit feeds, 8.1 times faster than a table (the G3507: 13.5), because the G3519's programmable CRC module is slower to feed (cause not tested). JPEG decode of the 160 x 120 picture: 6.52M cycles, 81.5 ms (the G3507: 6,373,566).

CMSIS-DSP 256-point complex FFT at 80 MHz: q15 73.4k cycles (0.918 ms), q31 257k (3.215 ms), f32 884k (11.055 ms). The LP-MSPM0L1306 took 72.1k, 246k and 822k cycles at 32 MHz: 1.8, 4.6 and 7.6 percent fewer, one flash wait state against two. Errors match the L1306 (6 and 12 LSB, 138 x1e-9).

Example sweep: 251 of the 259 make-route examples build (8 fail: 3 compile errors under TI Clang 5.1.1, 3 duplicate interrupt vectors, 2 FreeRTOS link-line errors). 107 ran on the board, none faulted (96 running, 11 stopped at their own end-of-example breakpoint). The other 143 built examples were skipped on purpose before the board was touched: 96 by the pin gate (78 use the display pins, PA15, PA25 or the DAC; 18 have no generated pin list to check), 24 low-power examples that can drop the debug link, 13 risky ones (boot configuration, keys, flash protection), 9 with a segment outside MAIN flash and SRAM, and 1 bootloader. "Running" means the core was still executing after 4 seconds; it does not mean the example's purpose was checked. TI's demo was restored and read back identical afterwards (main, data flash, NONMAIN). Porting with way 3: G3507 to G3519 builds 185 of 270, C1104 to G3519 74 of 91, G3519 to G3507 200 of 258, G3519 to C1104 88 of 258.

## Not measured, and why

The CSV lists each test that did not run with its reason: the 143 skipped sweep examples (the 96 behind the pin gate need the display wiring removed, the 24 low-power ones need a replug), power (needs a current shunt), the screen's picture (needs someone to look at it), the AI cost runs, and the UART1 loop (no wire).

## Security, core and control (2026-10-17)

Everything below ran from flash at 80 MHz; cycle counts come from SysTick on the chip, so the load on the Mac does not enter them. The post-quantum code is the CC1354P10 port with a SysTick timer on the CPU clock, because the Cortex-M0+ has no DWT cycle counter. Rows are in `data/results/mspm0g3519.csv`; the port is `portability/boards/mspm0g3519/PORT.md`.

**Post-quantum pack** (mlkem-native 2.0.0 and mldsa-native 2.0.0 unmodified, TI Arm Clang -O3, flash, three loads of three calls, identical to 0.01 percent; every NIST known-answer vector passed first):

| Step | Cycles | Time at 80 MHz | Stack | TI's published (library, 2 wait states) |
|---|--:|--:|--:|--:|
| ML-KEM-768 keygen | 1,969,936 | 24.6 ms | 14.9 kB | 2,162 k |
| ML-KEM-768 encaps | 2,330,313 | 29.1 ms | 20.5 kB | 2,514 k |
| ML-KEM-768 decaps | 2,850,622 | 35.6 ms | 19.2 kB | 2,864 k |
| ML-DSA-44 keygen | 6,573,571 | 82.2 ms | 32.7 kB | 6,688 k |
| ML-DSA-44 sign, one message | 25,395,160 | 317 ms | 51.8 kB | |
| ML-DSA-44 sign, mean of 16 messages | 41,258,295 (17.4 M to 104.7 M) | 516 ms | | 33,871 k (mean of 16, their messages) |
| ML-DSA-44 verify | 7,639,560 | 95.5 ms | 31.0 kB | 7,825 k |

The M0+ needs about 2.0 times the CC1354P10 (M33) cycles for ML-KEM. The generic C build is within 9 percent of TI's own library figures, so TI's library has no large advantage over mlkem-native here. The ML-DSA-44 sign mean over the 16 messages used here is above TI's because signing time depends on the message (rejection sampling), and those 16 include a 104.7 M one. Stack matched the M33 within 3 percent. Flash at -O3: ML-KEM-768 90.6 kB, ML-DSA-44 196.8 kB (Thumb-1 code is larger than the M33's), 11.5 kB and 16.6 kB at -Os (built, not run). The timer is SysTick on the CPU clock, one tick per cycle, no DWT (the M0+ has none).

**Classical crypto** (software on this chip; TI's MSP Crypto Library 0.33 prebuilt, and micro-ecc): SHA-256 of 1 kB 208.2 cycles per byte with TI's library (TI publishes 199) and 158.0 with the portable C of this repo. ECDSA P-256 sign 33.4 M cycles and verify 60.0 M with TI's library (TI publishes 31.4 M and 57.3 M); micro-ecc Thumb-1 assembly 16.6 M sign, 18.6 M verify, 15.7 M ECDH (matches an independent Python P-256 computation); micro-ecc portable C 38.2 M, 42.3 M, 37.1 M. TI's `Crypto_ECDH_getSharedSecret` returned 0x3C (X coordinate too long) for every input form tried, so that row is `failed`. A one-bit-flipped digest was rejected by every verify. `openssl speed` does not fit (no OS; flash shortfall 3.07 MB against libcrypto on the Mac).

**Core:** FreeRTOS maximum tasks 205 (heap 119.9 kB, 128-word stacks, 205 ran in both one-second looks, 432 B left).

**Control:** worst case and jitter of the existing four control builds, derived from the 2026-10-09 on-chip runs: the MATHACL float loop needs 909.6 percent of a 100 kHz period (jitter 7.2 us), the software float loop 1,819.9 percent, the fixed-point IQmath loops 125.9 percent (hardware) and 246.0 percent (software) with 0.3 us jitter. Power loop (the G3507's code, run 2026-10-17): Q15 highest rate 190.93 kHz (the G3507 held 190.93 too), CPU load 41.5 percent at 95.5 kHz, worst trigger to duty write 321 cycles (40.1 percent of a 100 kHz period); float highest rate 39.6 kHz, worst 1.9k cycles (236.8 percent). The extra 20 clocks of interrupt entry on this chip do not show in the power loop's limit, which the ADC sets.
