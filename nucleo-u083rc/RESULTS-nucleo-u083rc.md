# ST NUCLEO-U083RC results: CoreMark, hardware AES, control loop and ST's examples

**Short answer.** On ST's 48 MHz clock, CoreMark runs at 97 to 99 per second with GCC at -O3 with loop unrolling (2.03 to 2.06 per MHz). ST's own Clang with ST's tuned settings reaches 115 per second (2.40 per MHz), which matches the 2.4 per MHz in ST's data sheet (134 at 56 MHz; the data sheet does not name the compiler). The hardware AES block encrypts 1 kB 65 to 74 times faster than the tiny-AES-c software library when the code writes its registers directly, and 20 to 37 times faster through ST's HAL calls, about the same gain as the TI LP-MSPM0G3507 through TI's driver calls (ECB encrypt: 19.6 here, 24.5 there; the MSPM0 figures are from one run). All four room-occupancy models fit and pass. A changed blink took 7 min 51 s from plug-in on Zephyr (Zephyr already installed), and 14 s on ST's command-line tools after a 35-minute install.

Terms: [docs/vocab.md](../docs/vocab.md) and the board's [FAQ](FAQ.md). Start-here guide: [README](README.md). 1 kB = 1,024 bytes. Times in the tables are CPU cycles of the 48 MHz clock unless they say otherwise. BSRR is the GPIO set and reset register; Mass Storage is the probe showing up as a USB drive; loop unrolling is the compiler repeating a loop's body to save jumps. HAL is ST's driver library; LL is ST's lower-level layer. MSI is the chip's internal clock oscillator. ECB and CBC are two ways of running AES over many 16-byte blocks; FIPS-197 and SP 800-38A are the NIST documents whose test vectors prove an AES result is right. tiny-AES-c is a small open-source AES library in C; CMSIS-DSP is Arm's signal-processing library; Embench IoT is a suite of 19 small benchmark programs.

Measured 2026-10-08 and the early hours of 2026-10-09 on an Intel Mac, macOS 15.7.9, with other work running (load average 4 to 41 on 12 cores; every timing below is counted in CPU cycles on the chip, so Mac load does not move it). Board: NUCLEO-U083RC (MB1932), STM32U083 Rev A, onboard STLINK-V2EC (firmware V2J43M28), USB power, jumpers as shipped, no wires. Clock: ST's `SystemClock_Config()` copied unchanged from STM32CubeU0's `GPIO_IOToggle` example (MSI at 48 MHz, one flash wait state). Code in flash unless a row says RAM. Every cycle count is SysTick counting CPU cycles. Most tests ran three times and gave identical counts each time; single runs say so in the results file. Results are read out of RAM over the debugger. Every number is a row in [`data/results/nucleo-u083rc.csv`](../data/results/nucleo-u083rc.csv).

Software: STM32CubeU0 v1.3.0 (GitHub commit c87491e), STM32CubeCLT 1.22.0 (ST's GCC 14.3.1, ST's Clang 21.1.1, STM32_Programmer_CLI 2.23.0), Zephyr 4.4.2 with GCC 12.2 (the same GCC as this repo's other boards' gcc rows), xPack OpenOCD 0.12.0-7.

## Setup: plug-in to a changed blink

| Path | Plug-in or start to proven changed blink | Build | Flash | Notes |
|---|--:|--:|--:|---|
| Zephyr 4.4.2 blinky at 250 ms, OpenOCD | 7 min 51 s | 42.7 s | 1.2 s | Zephyr and GCC already on this Mac; includes fetching ST's sources (45 s) and about a minute on two OpenOCD flags |
| ST's command line (`./blink.sh 250`: ST's `GPIO_IOToggle`, ST's GCC, STM32_Programmer_CLI) | 14 s | 6 s | 5 s | After STM32CubeCLT was installed; the download and install took about 35 minutes of a person's time, an estimate (unsigned installer, two disk clean-ups) |
| STM32CubeIDE | not run | | | needs 6 GB of free disk; the Mac had 2.8 GB after STM32CubeCLT |
| STM32duino 3.0.0 with arduino-cli, on 2026-10-09 (core installed in the run: 80 s, 2.5 GB; uses ST's programmer from the 35-minute STM32CubeCLT install) | 2 min 4 s | 21 s | 3 s | The default upload (Mass Storage) fails on this board; use the SWD upload with ST's programmer on PATH (FAQ) |

Proof each time: LD4 (PA5) read over the debugger, toggling at 250 ms plus the time each halted read pauses the program. Flash used: Zephyr blinky 16,852 B (16.5 kB), ST's HAL blink 3,928 B (3.8 kB), Arduino Blink 13,180 B (12.9 kB).

## CoreMark

| Compiler and flags | Per second | Per MHz |
|---|--:|--:|
| ST's Clang 21.1.1, ST's `Omax.cfg` | 115.07 | 2.397 |
| ST's GCC 14.3.1 `-O3 -funroll-loops` | 98.82 | 2.059 |
| GCC 12.2 `-O3 -funroll-loops` | 97.35 | 2.028 |
| ST's GCC 14.3.1 `-O3` | 93.93 | 1.957 |
| GCC 12.2 `-O2` | 92.11 | 1.919 |
| ST's Clang 21.1.1 `-O3` | 87.93 | 1.832 |
| GCC 12.2 `-Os` | 71.45 | 1.489 |
| GCC 12.2 `-O3 -funroll-loops`, 4 MHz reset clock, no wait state | 8.216 | 2.054 |
| Zephyr 4.4.2, GCC 12.2 `-O3` / `-O2` / `-Os` | 92.18 / 88.14 / 70.70 | 1.920 / 1.836 / 1.473 |

Every run passed CoreMark's own check. Same compiler and files as the LP-MSPM0G3507's GCC rows (1.82 per MHz at 80 MHz, two wait states). ST's Clang needed ST's "hybrid" mode (its GNU linker scripts do not link with Clang's own linker); see the FAQ.

## Hardware AES, random numbers and CRC

| AES-128, 1 kB, GCC 12.2 -O3 | Software | HAL | Registers | HAL speed-up | Register speed-up |
|---|--:|--:|--:|--:|--:|
| ECB encrypt (cycles) | 396,953 | 20,214 | 6,093 | 19.6 | 65.1 |
| ECB decrypt | 723,750 | 20,309 | 10,125 | 35.6 | 71.5 |
| CBC encrypt | 404,954 | 20,249 | 6,111 | 20.0 | 66.3 |
| CBC decrypt | 754,921 | 20,348 | 10,143 | 37.1 | 74.4 |
| AES-256 ECB encrypt | 554,916 | 21,854 | 7,961 | 25.4 | 69.7 |

FIPS-197 and SP 800-38A vectors pass and the hardware output equals software over 1 kB. One 16-byte block: 142 cycles by registers, 637 through the HAL, 6,120 in software. The AES block chains CBC itself; the LP-MSPM0G3507 rows chain it in software, so compare ECB to ECB. At -O2 the software is slower and the register speed-up rises to 84 to 91 times. ST's own `CRYP_AESModes` example passes as shipped (experiment: [ST's AES example](../docs/experiments/2026-10-08-nucleo-u083rc-st-aes-example.md)).

Random numbers: 1 kB in 26k cycles (0.55 ms) by register polling, 50.2 percent ones, no repeated 32-bit word, no error flag.

CRC-32 over 1 kB: the CRC unit fed 32-bit words takes 1.8k cycles against 15,416 for a 256-entry table, 8.3 times faster. The table side matches the LP-MSPM0G3507's 15,430; the hardware side is 7.2 cycles per word.

## Core timing

| Test | Result |
|---|---|
| Dhrystone 2.1, GCC 12.2 `-O3 -fno-inline` | 0.672 DMIPS/MHz (0.479 at `-Os`) |
| Interrupt latency, bare metal | 22 to 26 cycles (pend to first handler line, including one SysTick read) |
| Interrupt latency, Zephyr | 108 to 129 cycles |
| GPIO, one BSRR store | 2.0 cycles at -O2 and -Os, 2.5 at -O3, the same assembly each time; Cause not found (no toggle register on STM32) |
| GPIO, `HAL_GPIO_TogglePin` | 21 cycles per pass |
| sinf plus cosf, software | 6.3k cycles |
| float divide, software | 443 cycles |
| FFT 256, CMSIS-DSP 1.10.0 from source | q15 1.443 ms, q31 3.744 ms, f32 19.97 ms (errors equal the LP-MSPM0L1306's) |
| Zephyr thread switch (k_yield round trip) | 18.8 to 21.8 µs |
| FreeRTOS V11.2.0 (same kernel and settings as the LP-MSPM0L1306), taskYIELD round trip | 9.29 to 13.25 µs (446 to 636 cycles) |
| FreeRTOS tasks that fit (48-word stacks, static) | 149 with the 40 kB linker script, run; 118 with ST's 32 kB one, built, not run |
| Embench IoT | 19 of 19 fit and pass at -O2 and -Os |

## Control loop

TIM1 at 1 kHz triggers the ADC, and the ADC interrupt runs one field-oriented control step (the same `foc.c` as the C2000 and MSPM0 boards, in software float) and writes a new PWM compare value. 1,000 interrupts per run, no ADC overrun.

| Build | FOC step (cycles) | Trigger to first ISR line | Trigger to new compare, mean and worst |
|---|--:|--:|--:|
| GCC 12.2 -O2, flash | 12,437 | 81 cycles (1.69 µs) | 298.8 µs, 319.6 µs |
| GCC 12.2 -O3, flash | 12,435 | 81 cycles | 299.3 µs, 319.8 µs |
| GCC 12.2 -O2, step in RAM | 11,408 | 76 cycles | 272.3 µs, 289.1 µs |
| ST's GCC 14.3.1 -O2 / -O3, flash | 12,970 / 12,960 | 81 cycles | |

The ADC's input is the internal reference, since no wire was fitted. The outputs match a Mac run of the same code.

## Analog (internal paths only; no wires fitted)

| Test | Result |
|---|---|
| Fastest ADC with DMA | 1.71 Msps at the shortest sampling time (24 MHz ADC clock, 12-bit). At that setting the internal reference reads low (); it reads true from 12.5 clocks, 0.96 Msps |
| DAC to ADC through the internal channel | Failed: the ADC read a flat 1.06 V for every DAC code. Suspected: the setup did not select the internal route. The wired loop (PA4 to PA0) has not been run |
| UART loop, half-duplex mode, no wire | Failed: no byte came back. Suspected: single-wire mode does not loop the transmitter back to the receiver. The wired loop (D8 to D2) has not been run |

## Room-occupancy models (plain C, 20 of 20 test vectors each)

| Model | GCC 12.2 -O3 | GCC 12.2 -Os | ST's GCC 14.3.1 -O3 |
|---|--:|--:|--:|
| occ_cnn_500 | 8.16 ms | 10.98 ms | 8.05 ms |
| occ_cnn_2k | 15.30 ms | 28.03 ms | 15.26 ms |
| occ_cnn_8k | 51.63 ms | 109.91 ms | 51.95 ms |
| occ_cnn_30k | 185.21 ms | 414.61 ms | 185.71 ms |

All four fit in ST's 32 kB linker region; the chip has 40 kB (experiment: [all 40 kB of RAM](../docs/experiments/2026-10-08-nucleo-u083rc-40kb-ram.md)).

## ST's examples

All 204 STM32CubeIDE projects for this board that are not secure-boot or option-byte projects built from the terminal with ST's GCC (6 skipped by rule). All 164 that were run started without a crash; the other 40 (low power, watchdogs, debug configuration) wait for someone at the board. The 6 skipped are secure-boot and option-byte projects.

## Limits

Not run: power (no meter for this board), the UART and DAC-to-ADC loops (they need two wires), the button examples (they need a finger on B1), the STM32CubeIDE path, the BitNetMCU digits and the post-quantum set. `data/results/nucleo-u083rc.csv` gives the reason for each.
