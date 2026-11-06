# Renesas FSP example projects on the LP-MSPM0G3519

Four of five Renesas FSP examples run on the LP-MSPM0G3519 LaunchPad kit: three with their application source exactly as published, one with five constants changed. The fifth starts and converts on a timer but does not move its samples. An I2C master example reads the touch controller on the Waveshare panel, a UART and PWM example sets a pin's duty from typed numbers, an ADC example reads A0, and a timer example runs periodic, PWM and one-shot modes. The timer-triggered ADC example (`adc_gpt_periodic_sampling`) gets its trigger through the chip's event fabric and a steady 21 microsecond sample interval, but its two-registers-per-trigger transfer to RAM fills only half of one group and none of the other. Renesas's `ra-fsp-examples` is BSD-3-Clause, so the code is copied as published and a 577-line layer maps the FSP calls onto TI DriverLib.

Terms: [vocab](../../docs/vocab.md). **FSP** is Renesas's Flexible Software Package, the driver layer behind `R_IIC_MASTER_Write` and the other `R_*` calls. **DriverLib** is TI's C library for the MSPM0 peripherals. **FT6336U** is the touch controller on the Waveshare 3.5 inch panel (I2C address 0x38). 1 kB = 1,024 bytes. Checked 2026-10-25 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS -Os, SysConfig 1.28.0, on one LP-MSPM0G3519 with the panel fitted, console on the probe's serial port at 115200.

## Results

| Example (Renesas folder `ek_ra6m5/...`) | Status | Flash | RAM | App edits | What ran |
|---|---|---|---|---|---|
| `iic_master` against the ADXL345 it was written for | Ran; wrong device | 12.6 kB | 516 B | 0 | Prints `Error in reading Device ID`, then loops with 65535 in every axis. No ADXL345 is on the bus. See the first row of "What went wrong" |
| `iic_master` retargeted to the FT6336U | Works | 12.6 kB | 516 B | 5 constants | Device-ID read returns 0x64 (matches), enable write and 6-byte read succeed; 3 runs of 16 s, every line identical (`X-axis = 2096.00, Y-axis = 356.00, Z-axis = 4608.00`, the FT6336U's registers 0xA1 to 0xA6) |
| `sci_uart` (UART input sets PWM intensity) | Works (pin read, LED not watched) | 13.1 kB | 567 B | 0 | 1, 25, 50, 75, 100 accepted; 0, abc, 101, 12.5 refused with the example's `Invalid input` text. A measurement build read PB27 for 100 ms after each value: 0.89, 24.90, 49.90, 74.90, 100.00 percent high, 100 to 101 rising edges. The LED was not watched |
| `adc` (single scan, menu over the console) | Works, single scan only | 13.3 kB | 535 B | 0 | `1` starts a scan and prints a count and volts (306 and 493 on two scans). A0 on PA27 is not wired, so the counts show the path runs, not a measured voltage. Continuous mode and the window-compare callback were not run |
| `gpt` (periodic, PWM, one-shot menu over the console) | Works (pins and counters read, LED not watched) | 14.9 kB | 581 B | 0 | All three modes and the example's range and menu errors as printed. PWM on PB27: 0, 0.89, 24.90, 49.90, 74.90, 100.00 percent high for 0, 1, 25, 50, 75, 100, and 1 kHz. Periodic: 100 ms measured 99.44 and 99.49 ms on the Mac's clock, 1 s 995.6 ms, 1 ms 0.994 ms (about 0.55 percent short at every setting). One-shot: 1000 counts took 89 cycles more than 1000 in 10 of 10 runs, and 16 million counts took the same 89 more in 5 of 5 |
| `adc_gpt_periodic_sampling` | Event link works; transfer half does not | 15.5 kB | 4.7 kB (4 kB is the sample buffers) | 0 | Timer event to ADC to DMA: 1.0k conversion intervals of 670 to 672 counts against 672 (21 microseconds), none missed. Two DMA channels per group on the ADC trigger: group A fills 256 samples per 512 triggers (238.1k of 476.2k captured), group B never completes. One channel: 238.1k of 238.3k captured (232 lost over 930 re-arms) |

Flash is text plus data from the size tool; RAM is `.data` plus `.bss` with the 512 B stack on top. The chip has 512 kB flash and 128 kB RAM. Exact byte counts are in `RESULTS-ROWS.csv`. The rows are copied into `data/results/mspm0g3519.csv`. The measurement builds (`FSP_SHIM_CFLAGS=-DFSP_SHIM_MEASURE`) are separate images from the ones sized in the table.

## What changed

- **Renesas source, unchanged:** `third-party/renesas/{iic_master,sci_uart,adc,gpt,adc_gpt_periodic_sampling}/src/`, with the upstream URL, commit `c74f31fde709a8b27571fee279ec584720fae4cd` and license in each `SOURCE.md` and `third-party/renesas/LICENSE`. The SEGGER RTT files are not copied.
- **Edits:** `patches/iic_master.patch`, one header, five changed lines: ID register 0x00 to 0xA3, signature 0xE5 to 0x64, enable register 0x2D to 0x00, enable bit 0x08 to 0x00, data register 0x32 to 0xA1. The other two examples build and run as published.
- **This repo's layer, `portability/fsp-on-mspm0/`:** 577 lines that every build can use: `include/hal_data.h` (141: FSP types and prototypes, including `timer_info_t`, `transfer_info_t`, `elc_cfg_t`, and `R_ADC0->ADDR[n]` as the ADC's result words), `src/fsp_mspm0.c` (228: the `R_IIC_MASTER_*`, `R_SCI_UART_*` and `R_ADC_*` calls, `SEGGER_RTT_printf` and `SEGGER_RTT_Read` on UART0, and the start-up that FSP's generated `main()` does), `src/fsp_mspm0_gpt.c` (97: `R_GPT_Open`, `Start`, `Stop`, `PeriodSet`, `DutyCycleSet`, `InfoGet`, `Close` on TIMG12, TIMA1 and TIMG0), `src/fsp_mspm0_adcgpt.c` (103: `R_ELC_*` as event-fabric channel 1, `R_DTC_*` as DMA channels on the ADC trigger, and the group-scan ADC calls), `include/SEGGER_RTT/SEGGER_RTT.h` (8). Another 122 lines exist only in measurement and experiment builds. The board half, `boards/mspm0g3519/`, is 189 lines: `board.syscfg` (64), `adc_gpt_periodic_sampling.syscfg` (79, the ADC as a four-conversion sequence on an event) and a hand-written `hal_data.c` (46) in place of the configurator's generated file.
- **The layer is driver code, not renames.** It holds driver logic (interrupt-driven I2C, a UART receive ring, a line-buffered console, PWM polarity and forced levels, timer modes with callbacks, DMA channel setup), so all 577 lines count as written code. The only 3-line stubs are `R_IOPORT_Open`, `R_BSP_IrqDisable` and `R_FSP_CurrentIrqGet`. The ELC and DTC mapping (about 70 of those lines) is the part that does not reproduce the RA behavior.
- **Not on this chip, as mapped:** RA group B starts on a second timer match at 99 percent of the period; the ADC has one start trigger per sequence, so groups A and B are one four-conversion sequence on one trigger (the chip's second ADC was not used).
- **Wiring:** touch controller on I2C2 SCL PC2 and SDA PC3 with its reset on PB11; console on UART0 PA10 and PA11 (probe serial port); PWM on TIMA1 channel 1 at PB27; ADC0 channel 0 on PA27; the `gpt` example's periodic and one-shot modes on TIMG12 (32 bits, 32 MHz) with the user LED on PA0 toggled each period; `adc_gpt_periodic_sampling` triggers from TIMG0 (672 counts, 21 microseconds) and converts PA27, PA26, PA25 and PA24 (nothing wired) into MEM4 to MEM7.

## Take it with you

The file list, what each needs, the board-table files to change for another MSPM0 board and the build command run from a fresh folder are in [`portability/fsp-on-mspm0/README.md`](../../portability/fsp-on-mspm0/README.md#take-it-with-you). For an example other than `sci_uart`, also copy that example's Renesas `src/` folder (`third-party/renesas/<example>/src/`) and, for `iic_master`, `patches/iic_master.patch`. An assistant can read [`portability/fsp-on-mspm0/AGENTS.md`](../../portability/fsp-on-mspm0/AGENTS.md). Built from a copy of only these files on 2026-10-25: `sci_uart`, 13.1 kB of flash and 567 B of RAM, the same as the table, no errors.

## Set up once

`portability/fsp-on-mspm0/build.sh` and `capture.py` read the variables below, fall back to the default shown, and stop with one line naming the variable when a path is missing.

| Variable | What it is | How to get it | Default folder | Size |
|---|---|---|---|---|
| `SDK` | MSPM0 SDK 2.11.00.07 (DriverLib, CMSIS headers, startup file) | TI's download page for the MSPM0 SDK | `/Applications/ti/mspm0_sdk_2_11_00_07` | 0.9 GB |
| `CGT` | TI Arm Clang 5.1.1.LTS (comes with CCStudio 21) | install CCStudio, or TI's compiler download | `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS` | 0.4 GB |
| `SYSCFG` | `sysconfig_cli.sh`, SysConfig 1.26.3 (1.28.0 is used if 1.26.3 is missing) | TI's SysConfig download, or the copy inside CCStudio | `/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh` | 0.2 GB |
| `OUT` | build folder, about 1 MB per example (about 2 MB for `adc_gpt_periodic_sampling`) | created by the build; pass any folder whose parent exists | any folder; set it, because the script's default is an external drive | 1 MB |
| `PORT` | the board's console, the probe's serial port | plug the board in, then `ls /dev/cu.usbmodem*`; the one ending in `1` is the console | `/dev/cu.usbmodemMG0000011` | |

Also needed: `pyserial` for `capture.py` (`python3 -m pip install pyserial`), a flasher for the `.out` file (UniFlash DSLite with `mspm0g3519/MSPM0G3519_<probe serial>.ccxml`, or the repo's `tools/ai_runs/portboard.py`, whose board table carries one probe serial that you change to yours), and the Waveshare 3.5 inch panel wired as in "What changed" for the I2C example. The Renesas source is already in `third-party/renesas/`, so nothing is cloned.

## Build

```bash
portability/fsp-on-mspm0/build.sh iic_master      # also sci_uart, adc, gpt, adc_gpt_periodic_sampling; NO_PATCH=1 for the unedited source; prints the size
```

The output is `$OUT/<example>.out` and `build.log` beside it. `FSP_SHIM_CFLAGS=-DFSP_SHIM_MEASURE` builds the measurement image: PWM pin reads, periodic tick markers, one-shot cycle counts, and for the ADC example the interval and sample-count report (add `-DONE_SHOT_COUNTS=16000000` for the long one-shot). A build takes under a minute.

## Flash and run

```bash
python3 tools/preflight_image.py mspm0g3519 <out>/iic_master.out    # an image that has not run on this chip before
python3 tools/ai_runs/portboard.py backup g3519 <dir>                # before the first write
python3 tools/ai_runs/portboard.py flash g3519 <out>/iic_master.out
python3 ports/renesas-ra-fsp/capture.py <log> 16 [--send "3:25\r"]
python3 tools/ai_runs/portboard.py restore g3519 <dir>               # at the end; reads back
```

`capture.py` opens the console first, then restarts the board (so the banner is not missed), and can send typed input: `--send "3:25\r"` types `25` and Enter 3 seconds after the reset. For `sci_uart` send a number from 1 to 100; for `adc` send `1` to start a scan (`--send "3:1\r"`); for `gpt` send the mode (`1`, `2` or `3`) and, for modes 1 and 2, the period in milliseconds or the duty in percent as the next line. `iic_master` and `adc_gpt_periodic_sampling` need no input. `STAMP=1` puts the host clock in front of each received line; `fit_periodic.py <log>` fits it to the periodic tick markers.

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| Unchanged `iic_master` prints `Error in reading Device ID` and then loops printing 65535 | The example returns success after a wrong device ID, so its main loop keeps reading a device that is not there | Treat the error line as the result; point the ID and data constants at the device that is on the bus |
| First I2C write after boot reports failure although the device is on the bus | The FT6336U keeps the bus busy while it starts after its reset line goes high; the transfer finished about 65 ms late and the example waits about 16 ms | Hold the touch reset low 5 ms, release it, wait 400 ms before the first transfer (in `main()` of the layer) |
| Device-ID read works, the next write never completes, status shows the bus busy | The controller's ACK bit was set on a read, so it acknowledged the last byte and the slave kept sending | Leave the ACK bit off on reads (the controller then sends the NACK that ends the read) |
| The I2C callback never sets the event the example waits for | The example clears its event flag after the call returns, so a callback fired inside the call is lost | Start the transfer in the call and finish it in the interrupt, as FSP does |
| Banner text is cut off after 160 characters | The console stand-in formatted into a 160 B buffer | Buffer is 384 B |
| Menu input does nothing | The UART receive interrupt was enabled only when an SCI UART was opened | Enable the UART interrupt at start-up and keep a 32 byte ring for the console |
| `DL_I2C_drainControllerRXFIFO` undeclared | It is not in this DriverLib | Read the FIFO with `DL_I2C_receiveControllerData` in a loop |
| OpenOCD attached to read a register restarts the program | `init` resets the core on this board | Measure inside the firmware (`-DFSP_SHIM_MEASURE`), not from the probe, while a program is running |
| A typed `25` is read as `2`, then `5` (menu and value prompts go wrong) | The console stand-in handed bytes over as they arrived; a real RTT line arrives whole | The stand-in is line buffered: `SEGGER_RTT_HasKey` is true once Enter is in, and `SEGGER_RTT_Read` returns that line |
| Duty 0 gives a pin that is high all the time | A compare value equal to the period leaves the pin at its start level, and a compare of 0 leaves one low count | Force the pin low for 0 percent and high for 100 percent (`DL_Timer_overrideCCPOut`) |
| A timer-triggered ADC converts once and stops | In sequence mode the ADC clears its enable bit after one sequence | Repeat mode, with the last conversion set to wait for the next trigger |
| The ADC converts but DMA never moves a sample | The DMA trigger passes results only when the DMA sample count is 1 | `DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1)` |
| DMA moves 256 samples, then nothing | The ADC clears its DMA enable bit when a transfer ends (ADC12 CTL2.DMAEN) | Set it again each time a channel is armed |
| Two DMA channels per group on one ADC trigger fill group A in 512 triggers and never finish group B | Cause not found; one channel on the trigger loses 232 of 238.3k samples | Not solved: a published-source port needs a layer design that gives each register its own trigger (the chip's second ADC, or a FIFO block transfer) or an edit to the application |
| The periodic timer measures 0.5 percent short on the Mac's clock | The 32 MHz oscillator runs about 0.55 percent fast on this board (same offset at 1 ms to 1 s) | Measure your board before relying on better than 1 percent |
| Banner text of `adc_gpt_periodic_sampling` stops mid-sentence | The console stand-in formats into a 384 B buffer and this banner is longer | Left as is; the example's logic is unaffected |
| Compiler warning in `adc_ep.c`: `snprintf` size 64 into a 5 byte buffer | Upstream code: `g_volt_str` is 5 bytes and the call passes 64; `"3.30"` fits, a longer string would not | Left as published; fix upstream if you reuse it |

## Effort

First three examples: under 2 hours from clone to running, including restoring the board, about 40 minutes of it on I2C transfer timing. The timer and ADC-with-timer examples: about 2.5 hours. The `gpt` example ran after one fix to the console stand-in and one to the 0 and 100 percent duty levels; the ADC example took five rebuild-and-flash rounds to find the three ADC and DMA settings above, then two experiments to see where the transfers go. Counted in lines: 577 written for the FSP calls (about 70 of them the ELC and DTC mapping), 189 of board configuration, 10 diff lines (5 constants) of Renesas source in 1 of 5 examples.

## Limits

Not checked:

- No LED was watched; PWM is checked on the pin by reading it, not by eye.
- ADC against a known voltage; continuous mode; window compare.
- The periodic timer's LED on PA0 and the PWM LED were not watched; the timer period was compared with the Mac's clock only, not with a crystal reference.
- `adc_gpt_periodic_sampling`: the transfer of two registers per trigger to two buffers (not solved above), group B on its own trigger, the second ADC, and sample values against a known voltage (the four pins are open).
- Other kits' example folders. The ek_ra6m5 copies were used; the same example names exist for other boards in the upstream tree, with different pin and clock settings.
- Other vendors' example packs. Infineon, Nordic, NXP, GigaDevice and Microchip example code is licensed for their own chips, so ports of those are written from behavior, not copied.

## License and source

Renesas `ra-fsp-examples` is BSD-3-Clause, Copyright (c) 2025 Renesas Electronics; the notice is in `third-party/renesas/LICENSE` and in each copied file's header. This repo's layer is separate code that uses FSP's public function names and does not contain Renesas source. Upstream: https://github.com/renesas/ra-fsp-examples at commit `c74f31fde709a8b27571fee279ec584720fae4cd` (`third-party/renesas/*/SOURCE.md`). MSPM0 SDK and TI Arm Clang are TI's, installed by you under their own terms.
