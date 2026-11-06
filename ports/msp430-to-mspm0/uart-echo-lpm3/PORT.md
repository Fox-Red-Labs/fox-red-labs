# Moving TI's MSP430FR2433 UART echo example to the LP-MSPM0L1306 and the LP-AM13E230

The UART set-up and the DCO trim loop were replaced by a SysConfig UART at 9600 baud, and LPM3 became STANDBY0 with the receive interrupt as the wake source. On the L1306 a 190-byte burst echoed unchanged from STANDBY0 when the UART used the bus clock. A UART on the 32 kHz clock, the only mode that receives in STANDBY1, echoed single typed bytes and lost most of a burst. The AM13 build was never run.

Terms: [docs/vocab.md](../../../docs/vocab.md), and the term list in the [folder README](../README.md). 1 kB = 1,024 bytes.

Source: `msp430fr243x_euscia0_uart_01.c` from TI's MSP430Ware code examples for the MSP430FR243x family, Ling Zhu, January 2018, BSD-3-Clause (notice kept in `original/`), from the GitHub copy `PiBoxY/msp430ware` commit 7c96db0. Original: DCO tuned to 8 MHz with a software trim loop, eUSCI_A0 at 9600 baud from SMCLK (auto-clock enable), receive interrupt echoes each byte, LPM3. Compiled for the MSP430FR2433: 392 B of FRAM, not run.

## What changed, by idiom

| MSP430 | Idiom | LP-MSPM0L1306 | LP-AM13E230 |
|---|---|---|---|
| `CSCTL1..3`, `DCOFTRIM`, `Software_Trim()` (about 60 lines) | `clock.fll_dco_trim` | deleted: SYSOSC is factory-trimmed | deleted |
| `UCA0CTLW0`, `UCA0BR0 = 52`, `UCA0MCTLW = 0x4900 \| UCOS16 \| UCBRF_1` | `uart.init_8n1` | SysConfig UART0, 9600 baud, 16x oversampling on the bus clock; or LFCLK at 3x (`example-lfclk.syscfg`) | SysConfig UART on UC1, PA0 and PA1, 9600 baud |
| `P1SEL0 \|= BIT4 \| BIT5` | `gpio.config_pin` | SysConfig PA8 transmit, PA9 receive | PA0 transmit, PA1 receive |
| `UCA0IE \|= UCRXIE`, `switch (UCA0IV)` | `uart.rx_wake_deep_sleep` | receive interrupt, FIFO on, threshold one entry, handler drains the FIFO | same, `APP_UART_0_INT_Handler` |
| `UCA0TXBUF = UCA0RXBUF` after `while (!(UCA0IFG & UCTXIFG))` | `uart.tx_byte_blocking` | `DL_UART_Main_transmitDataBlocking` | `DL_UART_transmitDataBlocking` |
| `__bis_SR_register(LPM3_bits \| GIE)` | `power.sleep` | `DL_SYSCTL_setPowerPolicySTANDBY0()` plus `DL_SYSCTL_enableSleepOnExit()` | wait loop (`SLEEP_MODE` 0); RUNSLEEP (1) built, not run |

Added: a start banner (after a 0.5 s delay so a debugger reset finishes first), LED toggle per byte, and `-DDEBUG_STATS` (prints bytes and interrupts on a newline).

## Expected and measured (LP-MSPM0L1306, 9600 baud)

| Variant | Expected | Measured |
|---|---|---|
| Bus clock, STANDBY0 (default build) | burst and single bytes echo | 190 of 190 bytes of one burst, 37 of 37, 3 of 3, and 24 single bytes 0.3 s apart all echoed unchanged |
| LFCLK UART, STANDBY0 | bursts echo (written before the run) | **wrong**: 5 of 38 bytes (the first and the last four); 23 single bytes 0.3 s apart and `abc` echoed |
| LFCLK UART, STANDBY1 | single bytes echo | 18 single bytes echoed and `abc`; 4 of 26 bytes of a burst |
| Bus clock, SLEEP (control) | burst echoes | 37 of 37 (console only) |

Why the 32 kHz UART loses a burst is not known: the same handler with the bus clock works, so the handler is not the cause. The chip counted 5 bytes and 1 to 2 interrupts for a burst of 38. A baud divisor of 1 plus 9/64 at 32768 Hz gives 9,575 baud, 0.26 percent below 9,600.

STANDBY0 was entered with the debug power request cleared (`mspm0x.dap dpreg 0x04 0` after every flash and reset).

## Not tested

A burst at the UART's other speeds, a continuous stream, STANDBY1 with the bus-clock UART (it cannot receive there), and the AM13 run. Current in any state.

## Size

L1306: 1.3 kB flash, 268 B RAM. AM13: 7.8 kB in bank 0. Original: 392 B of FRAM.
