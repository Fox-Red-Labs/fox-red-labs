# AI assistant notes for the TI LP-MSPM0C1104 LaunchPad development kit

The 1 kB of RAM decides almost everything: standard CoreMark, Dhrystone, a 256-point FFT and every room-occupancy model built here do not fit, and a one-task FreeRTOS blink leaves 16 bytes of RAM spare. This file is the short list of traps and limits, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0C1104 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C library for the chip's parts; **SysTick** is the Arm core's built-in timer; **hard fault** is the chip's crash stop; **Embench** is a set of 19 small test programs; **NONMAIN** is the flash area that holds the chip's boot settings. 1 kB = 1,024 bytes. Checked 2026-10-08 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0 and UniFlash 9.6.0, on one LP-MSPM0C1104 (probe firmware 3.0.0.26).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm0c1104/` or `data/` are in the Fox Red Labs repo; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0C1104: Arm Cortex-M0+ at 24 MHz, no PLL (`data/results/mspm0c1104.csv`) |
| Memory | 16 kB flash, 1 kB RAM (`mspm0c1104/README.md`) |
| Not on the chip | No FPU, no DAC, no math accelerator, no AES block; hardware CRC-16 only, no CRC-32 (`data/results/mspm0c1104.csv`) |
| SysTick | Does nothing: its registers read `0 0 0 0` and writes do not stick. Time with TIMG8 as a free-running 16-bit counter on the CPU clock (`DL_TIMER_CLOCK_BUSCLK`, wraps every 66k cycles) (`mspm0c1104/FAQ.md`) |
| LED | Red LED1 on PA22; TI's guide ties that pin to board version E3 or later, version E2 uses PA4 (`mspm0c1104/FAQ.md`) |
| Serial | UART0 PA27 (TX) and PA26 (RX), through shunts on header J101 rows 3 and 4 (`data/results/mspm0c1104.csv`) |
| Debugger | XDS110 on the board, USB; the probe used here shipped on firmware 3.0.0.26 (`mspm0c1104/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7 or later (the first with `ti_mspm0.cfg`), UniFlash 9.6.0 (`mspm0c1104/README.md`) |
| Probe serial | `ls /dev/cu.usbmodem*`, or `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | No `backup.py` recipe. Plan used on this board: with OpenOCD attached and halted, `dump_image backup-flash.bin 0x0 0x4000` (all 16 kB), read twice and compared with `cmp`. The shipped image is in `mspm0c1104/data/oob-flash.bin` |
| Build TI's blink | `cp -R <SDK>/examples/nortos/LP_MSPM0C1104/driverlib/gpio_toggle_output .` then `../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang` (3.1 to 4.3 s) |
| Flash | `openocd -f interface/xds110.cfg -c "adapter serial <serial>" -c "adapter speed 2500" -f target/ti_mspm0.cfg -c init -c "reset halt" -c "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" -c "reset run" -c shutdown` (0.6 s at 2.5 MHz, 1.3 s at the default 100 kHz; DSLite took 5.2 to 5.6 s) |
| Check | `python3 tools/check/blink.py --board mspm0c1104 --probe <serial>` |
| Restore | `program backup-flash.bin 0x0 verify` then `reset run` in the same OpenOCD form, then read back and `cmp` |

The pattern is the delay in `gpio_toggle_output.c`. The shipped demo blinks too, so a pin read is the proof (`data/results/mspm0c1104.csv`).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Example requires Rev E3 or later of the MSPM0C1104 LaunchPad` (SDK README), or a dark LED after a good flash | A version E2 board wires the LED to PA4; the example toggles PA22 (E3 and later) | Change the pin in `gpio_toggle_output.syscfg`, or watch J1 pin 8. Jumper J7 must be on | The pin toggles |
| FreeRTOS tasks never wake, no tick | SysTick does nothing on this chip | Put the tick on TIMG14 at 24,000 counts per tick, replacing the port's weak `vPortSetupTimerInterrupt` (`mspm0c1104/freertos/fr_common.c`) | A task runs on schedule |
| A second FreeRTOS task does not link | Asserts and the stack-overflow check are on | Turn both off; two tasks plus idle ran in 0.97 kB (`mspm0c1104/freertos/README.md`) | Link succeeds |
| Hard fault in `core_bench_list`, stack pointer `0x1FFFFF30` (reduced CoreMark with 400 or 200 B data) | Cause not found; the whole stack is used | Report no CoreMark score for this chip from these builds | Not fixed |
| UART wrong at 4 to 4.5 and 6.4 Mbaud, silent at 9 and 12 Mbaud | Not traced | Test your exact rate; 8 Mbaud (24 MHz / 3) was right | Echo matches |
| LED already blinking before the first flash | The shipped demo blinks about every 0.5 s | Sample the pin every 25 ms or faster | Toggles seen |
| `Warning: A firmware update is required for the XDS110 probe. The current firmware is version 3.0.0.26` | The probe shipped on old firmware | DSLite still flashes and verifies; OpenOCD printed nothing. Update only if a tool refuses | Flash verifies |
| `Could not find MEM-AP` after running a ported G3507 example (`tima_timer_mode_pwm_dead_band`, 2026-10-24) | Boot loop: BOOTDIAG cycles (0x07 = nested exception) and the loop resets the debug logic, so `recover_halt.tcl`, the DSSM wait-for-debug and power-AP holds do not stick | DSSM factory reset (command 0x020A; erases main flash, NONMAIN to defaults; approval first), write `mspm0c1104/data/oob-flash.bin` back, then start with an nRST board reset. The recovery table at the end of this file has the sequence | PA22 toggles every 498 ms (2026-10-24) |

Traps shared with other boards: TRAPS.md.

## What fits and what does not

| Program | Result | Source |
|---|---|---|
| TI blink | 424 B flash, 64 B RAM, all stack | `data/results/mspm0c1104.csv` |
| Standard CoreMark | does not fit: 20.2 kB of code in 16 kB of flash, a 2.5 kB stack in 1 kB of RAM | `data/results/mspm0c1104.csv` |
| Reduced CoreMark (400 and 200 B data) | links, then hard faults; cause not found | `mspm0c1104/FAQ.md` |
| Dhrystone 2.1, 256-point q15 FFT | do not fit (arithmetic, not linked) | `data/results/mspm0c1104.csv` |
| Embench-IoT | 9 of 19 link; 7 run and pass at -O2, 6 at -Os | `data/results/mspm0c1104.csv` |
| FreeRTOS V11.2.0, one task plus idle | 4.1 kB flash, 0.98 kB RAM | `mspm0c1104/freertos/README.md` |
| Room-occupancy models (636 to 32.8k parameters) | none link; the smallest needs about 1.2 kB of buffers | `data/results/mspm0c1104.csv` |
| BitNetMCU 1k handwriting model | runs, 10 of 10 test digits right, 7.2 kB of code, 804 B of RAM | `data/results/mspm0c1104.csv` |
| Zephyr | not supported: 1 kB of RAM, no Zephyr support | `data/results/mspm0c1104.csv` |

## What ran here

- Changed blink: 77 s from first command, tools installed, proof by pin read, not by eye (`data/results/mspm0c1104.csv`).
- GPIO store 2 cycles; interrupt latency 23 to 25 cycles (`data/results/mspm0c1104.csv`).
- ADC 1.0 million samples a second with TI's DMA example at 10 bits; 0.84 million at 12 bits (same file).
- Hardware CRC-16 over 256 bytes: 1.1k cycles, 3.0 times faster than a 256-entry table (same file).
- FreeRTOS task switch round trip 478 cycles, 19.9 us (`mspm0c1104/freertos/README.md`).
- SDK sweep: 90 of 91 examples build; 68 ordinary and 15 low-power examples ran without a crash (`data/results/mspm0c1104.csv`).
- Not run: the CCStudio route, power, AI cost runs (`data/results/mspm0c1104.csv`).

## Check a blink without looking

After a flash, prove the LED pin is toggling through the debugger: `python3 tools/check/blink.py --board mspm0c1104 --probe <your probe serial>` from the repository root (xPack OpenOCD; it reads the GPIO registers for 3 s and never writes). PASS means an LED pin enabled as an output changed state; its pins come from TI's blink example for this board; it has not run on this board. This is the check to paste into a report.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| BitNetMCU in 1 kB of RAM on the LP-MSPM0C1104 | Works | The 1k handwriting model reads all 10 test digits right in 804 B of RAM on the LP-MSPM0C1104. | `docs/experiments/2026-10-08-mspm0c1104-1kb-ram.md` |
| MSPM0 examples moved up and down the family | Works | MSPM0 examples move between family members with only the SysConfig device changed (SysConfig is TI's pin and peripheral setup tool): all 133 images that built and write nothing outside main flash and SRAM ran on the LP-MSPM0L1306. | `portability/README.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `mspm0c1104/README.md`, then `mspm0c1104/FAQ.md` for each problem, and `data/results/mspm0c1104.csv` for every number. If this file gets something wrong on your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A program that faults before main (a write that stopped short): the fault handler faults too and the chip resets in a loop; MEM-AP is up only briefly. The chip has no ROM bootloader (TRM SLAU893C Table 1-6, no BSL support) (`docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, `mspm0c1104/FAQ.md`, 2026-10-19)
- The probe left nRST held low after a connect under reset; every connect fails until it is released (`docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19)
- Second lockout after the ported tima_timer_mode_pwm_dead_band: the debug port answers, BOOTDIAG cycles (0x07 = nested exception, CCStudio GEL) and the loop resets the debug logic too, so AP0 flickers and power-AP holds do not stick; restored 2026-10-24 by DSSM factory reset (`mspm0c1104/AGENTS.md`, 2026-10-24)
- Third lockout after the ported smbus_smbus_controller (G3507 port): the debug port answers (DPIDR 0x6ba02477), the core is not examined (CPUID reads 0x0), the same signature as the second lockout; restored 2026-10-25 by DSSM factory reset, oob-flash.bin written back (2026-10-25)
- Fourth lockout after the ported L1306 spi_controller_internal_loopback_poll (it passed preflight; SPI0 on PA2, PA11, PA16, PA18): repeated external reset, the debug port answers (DPIDR 0x6ba02477) and no MEM-AP is found; recovered the same day by the DSSM factory reset (0x020A), oob-flash.bin written and read back equal, nRST start, PA22 497 ms (2026-10-25)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Check OpenOCD's nRESET line; if 0, adapter deassert srst (for the held-nRST case) | remote | proven | `docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19 |
| bench/recover_halt.tcl (wait-for-debug writes, vector catch, halt at first instruction), then program with OpenOCD (for the reset loop) | remote | proven | `mspm0c1104/bench/recover_halt.tcl`, `docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19 |
| bench/recover_halt.tcl, about 7 variants and 72 halt-and-program retries (second lockout: DHCSR stays 0, the core never halts) | remote | failed | `mspm0c1104/FAQ.md`, 2026-10-24 |
| Unplug USB for 5 s, replug, run recover_halt.tcl again (one try) (second lockout) | hands | failed | `mspm0c1104/AGENTS.md`, 2026-10-24 |
| uhubctl power cycle of the probe's hub port, 30 s, four times (second lockout; the probe re-enumerated each time, whether VBUS dropped is unknown) | remote | failed | 2026-10-24 |
| DSSM mass erase (0x020C) (the C1104 does not answer it; CCStudio's GEL says mass erase is not available) | remote | failed | `mspm0c1104/FAQ.md`, 2026-10-15 |
| DSSM wait-for-debug (0x0206) through the SEC-AP mailbox, then nRST (bench/lockout_diag.tcl c1104_wfd) (second lockout: the ROM answers (RDR 0x00010003) but the loop continues; CPUID reads 0) | remote | failed | `mspm0c1104/bench/lockout_diag.tcl`, 2026-10-24 |
| Power-AP hold as OpenOCD's ti_mspm0.cfg does it (DPREC 0x104008, SPREC 1), 400 writes (second lockout: DPREC reads back its reset value) | remote | failed | 2026-10-24 |
| DSSM factory reset (0x020A): the command TI documents for this chip, erases main flash and resets NONMAIN; the core may still need the wait-for-debug hold afterwards (second lockout: the ROM answered (RCR 0x0A, RDR 0x00010003), the loop stopped, main flash read all 0xFF and NONMAIN TI's default; then write oob-flash.bin and start with an nRST board reset. Erases main flash; back it up first) | remote | proven | `mspm0c1104/bench/lockout_diag.tcl`, 2026-10-24 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-24. Four 30 s cycles; the probe re-enumerated each time, whether VBUS dropped was not measured.

Restore: xPack OpenOCD (flash write_image erase, verify_image); back up before every ported example. Regions: main flash (oob-flash.bin); NONMAIN is left as is (factory reset policy only, Type B).

<!-- recovery:end -->
