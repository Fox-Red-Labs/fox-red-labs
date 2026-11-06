# Do Cmsemicon's CMS32F04x example programs run on a TI LP-MSPM0L1306, and what did it take?

Yes. 34 of the 53 example variants in Cmsemicon's CMS32F04xx software pack build for the LP-MSPM0L1306, and all 34 ran on it on 2026-10-07: 29 with their `main.c` compiled exactly as Cmsemicon shipped it, 5 with 1 to 15 lines edited. What it took was one glue layer written once, 845 lines of C and 409 of headers, that gives Cmsemicon's function names a meaning on TI's DriverLib, plus one pin-map file. The 19 variants not built need hardware the L1306 does not have (CAN, segment LCD, USB fast charge, a clock monitor) or a second device on the bench (12 I2C and SPI examples). Behavior was checked on the board for 21 of the 34: text on the serial port, the LED pin toggling, timer counters moving, a CRC that matches a host computation, a flash write read back, wake-up from deep sleep. The other 13 ran without crashing but have nothing a probe can see (an op amp with no signal, a scan into RAM). One example prints a wrong temperature, and one watchdog example locked the debugger out until the glue was changed.

Terms: [docs/vocab.md](../../docs/vocab.md) and the [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). On this page: a *Keil pack* is the zip of device files, drivers and examples a chip vendor publishes for Arm's Keil tools; *glue* is code that implements one vendor's function names on another vendor's driver library; *DOUT* is GPIOA's output register, read to see what the LED pins are doing; *RESF* is Cmsemicon's reset-cause register; *-O0* and *-O2* are the compiler's optimization levels, off and normal; an *ISR* is an interrupt handler; *GBK* is the Chinese text encoding the pack's files use; *DSLite* is the command-line debugger inside TI's CCStudio, used here to flash; a *mass erase* wipes the chip's main flash through its debug port; *TIMG0*, *TIMG2* and *TIMG4* are three of the L1306's four 16-bit timers, *COMP0* its one comparator, *WWDT0* its window watchdog; *STOP0* and *SLEEP0* are two of the chip's low-power modes, deep and light; *I2C*, *SPI* and *CAN* are the usual serial buses. DriverLib, SysConfig, OpenOCD, DMA and PWM are in the vocab page. 1 kB = 1,024 bytes.

Done 2026-10-07 on an Intel Mac, macOS 15.7.9: TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, TI DSLite 21.0.0.4166 (from CCStudio 21.0.1) for flashing, xPack OpenOCD 0.12.0-7 for reading pins and registers, one LP-MSPM0L1306 (probe <probe serial>). Source: `Cmsemicon.CMS32F04xx.0.1.5.pack` from mcu.com.cn (a zip file; unpack it yourself). Its examples and driver headers are GBK-encoded with Chinese comments. The pack carries no license file, so this copy does not redistribute it: unpack the pack's `Examples` and driver headers into `ports/cms32/upstream/` before running `build.sh` (the path layout `build.sh` expects is `upstream/examples/<example>`).

Contents: [What a Cmsemicon example looks like](#what-a-cmsemicon-example-looks-like), [How the port works](#how-the-port-works), [Results](#results), [What the L1306 could not do](#what-the-lp-mspm0l1306-could-not-do), [The watchdog lock-out](#the-watchdog-lock-out), [Mistakes to avoid](#mistakes-to-avoid), [Repeat it](#repeat-it).

## What a Cmsemicon example looks like

The pack has 26 example folders holding 53 variants (53 `main.c` files). Each variant is a `main.c` plus a few "demo" files (`uart_demo.c`, `gpio_demo.c`, `tim_demo.c`, `delay_demo.c`, `retarget.c`) that sit between the application and Cmsemicon's driver library. `main.c` holds the idea of the example in 30 to 130 lines and calls the demo files by name (`Uart0_Init(19200)`, `Led_Init()`, `TIM1_Config()`, `Intp_Init()`, `delayMS(250)`). The demo files hold the chip detail: pin choices, register writes (`SCI0->SDR00 = *pTxbuff`), interrupt plumbing. A few `main.c` files poke port registers directly (`PORT->PB ^= (1<<8)`).

The port uses that split as the seam: the demo files are replaced, `main.c` is not.

## How the port works

Three layers, none of them a rewrite of the examples:

1. **Application: `main.c` as shipped.** 29 of the 34 variants compile their `main.c` byte for byte from the pack (the build transcodes the file from GBK to UTF-8 for the compiler, nothing else). Five got an edited copy in `ported/`, each change marked `/* edited */`. Counted as upstream lines changed or removed, of the file's total lines: Blinky 4 of 73, Gpio_input 15 of 100 (13 of them register writes that set every port to pulled-up input), ramfetch 2 of 78, clkselect 1 of 73 (those four files write port registers directly), and flashEraseWrite 4 of 129 (it reads flash through raw pointers at addresses above the L1306's 64 kB). 26 lines in all.
2. **Glue: the demo-file names and the driver names, written once on DriverLib.** `glue/l1306/` has 845 lines of C (1,021 counting a copy of TI's startup file) and 409 of headers. `CMS32F04xx.h` is a stand-in for Cmsemicon's device header: the same enums, the same function names (`PORT_Init`, `ISR_Register`, `EraseSector`, `CGC_Fclk_Select`), and the read-only registers the examples look at (`RST->RESF` for the reset cause, `CGC->CKC` for the clock switch, `TSN->TSN25` for the temperature trim) as shadows the glue fills in. Each `*_demo.h` keeps Cmsemicon's interface and is implemented on the L1306. The glue hooks into TI's C runtime (`_system_post_cinit`) to power the GPIO block before `main()` runs, so `main.c` does not need a call added.
3. **Configuration: where each Cmsemicon pin lands,** in one file, `glue/l1306/cms_pinmap.h`. The examples' LED (PB8, PB6, PF3, PF4, PA4 on various boards) becomes LED1 on PA0; their second LED (PB7) the RGB blue on PA27; their key input the S2 button on PA14; ADC channel numbers map one to one; UART0, "UART1", "UART2" and "USART1" all land on UART0, the LaunchPad kit's USB serial port, so every example's text can be read from the Mac.

Two build facts that matter beyond this port:

- **The examples only work unoptimized.** `main.c` files poll a plain `extern uint32_t g_ticks` that an interrupt decrements. At -O2, TI Arm Clang (correctly) turns `while (g_ticks);` into an endless branch and the LED never moves. Cmsemicon's own Keil projects build at optimization level 0, so the port compiles `main.c` at -O0 and the glue at -O2. The examples, not either toolchain.
- **A weak handler in the glue loses to TI's startup file.** TI's `startup_mspm0l130x_ticlang.c` aliases every handler, including `SysTick_Handler`, to a default endless loop with the `weak` attribute. A second weak definition in the glue did not win, so the port carries a copy of that startup file with the one alias removed; the glue's or the example's `SysTick_Handler` is then used.

The build is one script, `build.sh <example>`; the run is one script, `run.sh <example> <seconds> <baud>`, which flashes through TI's DSLite, records the USB serial port with `capture.py`, and reads DOUT five times at uneven gaps through OpenOCD (a toggling LED shows as changing bits). `probe.sh` halts a running example to read its program counter and chosen registers. `run_all.sh` does all 34, sleep examples last. What each example should show was written before the first run.

## Results

Rungs are those of `portability/README.md`: unchanged (`main.c` as shipped), edited, ran, verified. Bit 0 of DOUT is LED1 (PA0). Each row below comes from the flash log, the serial text, the DOUT reads and, where needed, halted register reads.

| Example | main.c | Ran | What the board showed | Verified |
|---|---|---|---|---|
| systick | unchanged | yes | LED1 toggles every 250 ms: DOUT bit 0 differed across the five reads | yes |
| Blinky | 4 of 73 lines edited | yes | PA0 and PA1 toggle together (DOUT 3, 0, 0, 0, 3) | yes |
| Gpio_input | 15 of 100 edited | yes | program in its delay loop (probe); the 100 ms pulses were not caught by the five reads | ran |
| ramfetch | 2 of 78 edited | yes | PA0 toggles with the core at 4 MHz (the chip's nearest to the example's 6 MHz) | yes |
| clkselect | 1 of 73 edited | yes | `Hello, UART`, `Select SubOSC as system clock`, then PA0 toggles slowly with the CPU on the 32 kHz clock | yes |
| CRC | unchanged | yes | `CrcResult` in RAM = 0x005D4625, which equals the host script's byte-wise, unreflected CRC-32 of words 0 to 15. The block feeds bytes, so a word-wise CRC (0xEB99FA90) is not the match. Cmsemicon's own value is unknown, there is no CMS32F040 here. An earlier probe read 0 before the CRC block was powered (see below) | yes |
| softReset | unchanged | yes | `RESF = 0x00` on the first boot, `RESF = 0x04` (software reset) on every boot after, about 300 lines in 5 s | yes |
| intp | unchanged | yes | LED1 on for 500 ms, then off; the S2 press was not run | ran |
| iwdg | unchanged | yes | 250 ms watchdog never fed: DOUT read 0x1 (LED1) once, then 0x08000000 (PA27, the RGB blue) four times, so the chip rebooted and the reset-cause branch took the other LED; the four-times-a-second rhythm itself was not timed | ran |
| wwdg | unchanged | yes | with the window opened by the glue (see the lock-out below): LED1 steady on across all five reads, no reset seen | ran |
| itIntervalTimer | unchanged | yes | `IntervalTimer operates with fIL`, then `15bit IntervalTimer interrupt: No.00` to `No.06` in the saved 6 s capture (the example prints twice a second, so No.02 and No.04 appear twice), LED1 toggling | yes |
| flashEraseWrite | 4 of 129 edited | yes | all four lines: `Sector 1 Erase/Write Done`, `Sector 2 Erase/Write Done`, the 41-character string read back from flash, `Byte Write is DONE!` (the run after the two fixes below; the first run stopped after line 2, the second printed a garbled string and `Byte Write is ERROR!`) | yes |
| STB/sleep | unchanged | yes | LED1 toggles from the timer interrupt and on each wake from SLEEP0 | yes |
| STB/deepsleep | unchanged | yes | 20 toggles, then STOP0; bit 0 changed across reads; the board stayed reachable | yes |
| STB/deepsleep_itWakeup | unchanged | yes | `Hello, IT Wakeup test!`, `enter Deepsleep!`, `Deepsleep Wakeup!`: woken from STOP0 by the 32 kHz timer | yes |
| uart/uartSendReceive | unchanged | yes | `Cmsemicon SCI0 transmit & receive test !` 96 times in 3 s at 115200 | yes |
| usart/usart_printf | unchanged | yes | `Hello, USART` | yes |
| usart/usart_dma | unchanged | yes | `Cmsemicon USART1 DMA transmit test!` every 500 ms by DMA | yes |
| timer/timer1 | unchanged | yes | TIMG0 counting (0x359 then 0x3E4, 50 ms apart) at the 17.57 kHz PWM period; 2 of the original 4 channels | yes |
| timer/timer3 | unchanged | yes | LED1 toggles on each compare interrupt (bit 0: 0, 1, 0, 1, 0) | yes |
| timer/timer14_timing | unchanged | yes | PA0 toggles every 5 ms from TIMG4 | yes |
| timer/timer16 | unchanged | yes | capture timer counting (0x1B47); `uwTIM16Freq` 0 with nothing on PA10 | ran |
| timer/timer17 | unchanged | yes | TIMG2 counting at the PWM period; no dead band on this chip | yes |
| timer/timer1_dma | unchanged | yes | TIMG0 counting; the compare register did not show the DMA duty table, so DMA into a timer register is not proven | ran |
| timer/timer14 | unchanged | yes | the 5 ms periodic variant: PA0 toggling | yes |
| adc/adcSoftTrigger | unchanged | yes | `Hello, UART`, 8 readings and the average every second; A2 (PA25) floating reads 0x0000 to 0x0002 | yes |
| adc/adcHardTrigNoWait | unchanged | yes | readings arrive in blocks of 8 by timer-triggered DMA (0x0003, 0x0000, ...) | yes |
| adc/adcSoftTrigger_dma | unchanged | yes | `Hello, UART`; the DMA buffer holds zeros, the same as the floating inputs read elsewhere, so the fill cannot be told from untouched RAM | ran |
| adc/adcTempSensor | unchanged | yes | `ADC Get Value = 806, Voltage = 0.63V, temperature = 453`. The reading is real, the temperature is not: the glue's two trim constants are placeholders, see below | ran |
| adc/adcHardTrigNoWait_Scan4Channel | unchanged | yes | `Hello, UART`, then the 4-channel scan runs silently as designed | ran |
| acmp/ACMP0 | unchanged | yes | COMP0 on, program in its loop; nothing on the inputs to see | ran |
| acmp/ACMP1 | unchanged | yes | same, on the one comparator | ran |
| OP/OP0, OP/OP1 | unchanged | yes | op amp on, program in its loop | ran |

Totals: 34 built, 34 ran, 21 verified, 13 ran only, 5 edited (26 lines in all). Images are 1.8 kB (OP/OP0 and OP/OP1, close to the glue's own floor) to 11.1 kB (adcTempSensor, floating-point `printf`) of 64.0 kB. RAM is 1.0 to 1.1 kB of 4.0 kB for 33 of them (a 768-byte stack plus the UART and interrupt tables), and 2.1 kB for flashEraseWrite, whose example holds two 512-byte buffers.

## What the LP-MSPM0L1306 could not do

Not built, each with its reason. The first four rows are chip limits by the MSPM0L1306 data sheet (SLASEX1, the peripheral list on page 1); the last row is a bench limit.

| Examples | Why |
|---|---|
| `can/*` (3 variants) | No CAN on the MSPM0L1306. The MSPM0G3507 has CAN-FD; the CMS32F042 is the only CMS32F04x with CAN |
| `lcd/4COM_7SEG_LCD` | No segment LCD driver on this chip. The MSPM0L2228 has one |
| `PD`, `QC` | No USB fast-charge negotiation block on any MSPM0. |
| `SCM` | No oscillation-stop detector on the L1306. |
| `iic/*` (6), `spi/*` (6) | They need a second device (EEPROM, W25Q flash, a second board) that is not on the bench. The glue has the DMA and bus names; these are the next round |

Built and run with a known gap, all from the same data sheet unless marked:

- `timer/timer1` drives 2 PWM channels, the original 4 with complementary outputs: TIMG0 has two channels and no complementary pair. `timer/timer17` has no dead band or break input on TIMG. `timer/timer3` schedules 2 compare channels, not 4. `timer/timer14` chains three gated timers in the original; here it is the 5 ms periodic variant. The MSPM0G series' TIMA covers all four (dead band, fault input, 4 channels); the L series has TIMG only.
- `acmp/ACMP1` lands on the same COMP0 as `ACMP0`: one comparator on the L1306, two on the CMS32F040.
- `adc/adcTempSensor` prints 453 for the temperature. Cmsemicon's formula needs the ADC codes at 25 C and 85 C; the glue holds two fixed numbers (`cms_core.c`, `cms_tsn`) so the formula runs, and they are not from this chip. A calibrated port needs TI's trim constant and the data sheet slope. Separately, the L1306's own one-point trim gave -165 C on 2026-10-06 ([LP-MSPM0L1306 README](../../mspm0l1306/README.md), "The chip").
- `flashEraseWrite`: the example's addresses (0x1F000 and up) are beyond the L1306's 64 kB. The glue folds addresses it is handed down by 64 kB; the example's own raw pointer reads needed the 4-line edit. MSPM0 sectors are 1 kB, the CMS32F040's 512 bytes, so two of the example's four erases wipe a neighbour it had just written. The two "Done" lines print unconditionally in the example; the last check reads 0xF600 and passes.
- `iwdg` runs on WWDT0 in plain watchdog mode: the L1306 has one watchdog; the CMS32F040 has an independent one and a window one.

## The watchdog lock-out

The `wwdg` example, run as written, refreshes the window watchdog in a tight loop inside the closed window, so the chip reset itself every few milliseconds from the first boot on. From then on OpenOCD answered every connect with `Could not find MEM-AP to control the core`, and DSLite with `Connection to MSPM0 core failed. Possible root causes: 1) Debug access within NONMAIN was disabled or enabled with password. 2) Peripheral mis-configuration (e.g improper watchdog or clock)`. Every run queued behind it did nothing. The recovery that worked from the terminal:

```bash
openocd -f interface/xds110.cfg -c "adapter serial ML130001" -f target/ti_mspm0.cfg -c "mspm0x.cpu configure -defer-examine" -c init -c mspm0_mass_erase -c shutdown
```

The mass erase goes through the chip's debug mailbox and succeeds while the core is unreachable; the script then throws a Tcl error from its reset helper, which can be ignored. The glue now configures the window fully open, so the example runs without resetting.

## Mistakes to avoid

- Build `main.c` at -O0 and the glue at -O2. At -O2 the tick-polling loop becomes an endless branch and nothing toggles.
- Set the serial speed and read in one open. On macOS, `stty` settings do not survive from the command that sets them to the command that reads, and the capture comes back as garbage. `capture.py` does both in one open.
- Read a blinking pin at uneven gaps. Reads a fixed 1 s apart land in the same phase of a 500 ms blink and report a steady pin.
- Do not map a button to a pin an example uses as an LED. PB0 as S2 stopped Blinky's first LED; the key is wired in the interrupt glue instead.
- Write MSPM0 flash in 8-byte words. The chip refuses a second write into the same word, so 4-byte writes leave every other 4 bytes at 0xFF (the garbled string and `Byte Write is ERROR!`).
- Power the CRC block before use. It reads 0 until powered; TI's generated config does that in `SYSCFG_DL_initPower`, which the glue does not use.
- In `ADC_Converse`, wait on either the status flag or the interrupt's completion: the handler clears the flag, and waiting on the flag alone hangs.
- Open the window watchdog fully before running `wwdg` (the glue does now; see the lock-out above).
## Repeat it

```bash
cd ports/cms32
./build.sh systick                      # any folder under upstream/examples
./run.sh systick 4                      # flash, 4 s of serial, five pin reads
./run_all.sh                            # all 34, about 12 minutes
```

Needs the tools above, the unpacked pack in `upstream/` and the LP-MSPM0L1306 on USB (`run.sh` and `build.sh` are written for probe serial <probe serial>; change it to yours). `run.sh` refuses to start if the probe is missing; it never touches another board.
