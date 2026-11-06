# Moving TI's MSP430FR2433 LPM3 watchdog-timer example to the LP-MSPM0L1306 and the LP-AM13E230

The watchdog in interval mode became WWDT in interval mode on the 32 kHz clock, LPM3 became STANDBY0, and the crystal set-up was deleted. On the L1306 the wake period was 0.997 s and the CPU clock stopped in the sleep. On the AM13, STANDBY0 with only that timer as wake source never woke, and the debugger could not connect afterwards.

Terms: [docs/vocab.md](../../../docs/vocab.md), and the term list in the [folder README](../README.md). 1 kB = 1,024 bytes.

Source: `msp430fr243x_LPM3_01.c` from TI's MSP430Ware code examples for the MSP430FR243x family, William Goh, January 2014, BSD-3-Clause (notice kept in `original/`), read from the GitHub copy `PiBoxY/msp430ware` commit 7c96db0. Original: `WDT_ADLY_1000` (1 s from ACLK on the 32768 Hz crystal), XT1 on P2.0 and P2.1, LPM3, the watchdog interrupt toggles P1.0. Compiled for the MSP430FR2433: 122 B of FRAM, not run.

## What changed, by idiom

| MSP430 | Idiom | LP-MSPM0L1306 | LP-AM13E230 |
|---|---|---|---|
| `WDTCTL = WDT_ADLY_1000`, `SFRIE1 \|= WDTIE` | `timer.interval_watchdog_as_timer` | WWDT0 interval mode, divider 1, 15 bits (1 s at 32768 Hz), interrupt on group 0 | SysConfig WWDT interval, same settings, handler `APP_WWDT_0_INT_Handler` |
| `P2SEL0`, `CSCTL4 = SELA__XT1CLK`, fault-flag loop | `clock.low_freq_aclk` | deleted; WWDT runs from LFCLK (internal LFOSC) | deleted |
| `__bis_SR_register(LPM3_bits \| GIE)` | `power.sleep` | `DL_SYSCTL_setPowerPolicySTANDBY0()` and `__WFI()` in a loop on a tick counter | `DL_SYSCTL_setPowerPolicySTANDBY0()` (hung) or `STOP` (`-DDEEP_POLICY=STOP`, built, not run) |
| `PM5CTL0 &= ~LOCKLPM5`, `PxDIR = 0xFF` | `gpio.release_after_lowpower_reset`, `gpio.unused_pins_safe` | `Board.configureUnused` | removed |
| `P1OUT ^= BIT0` in the ISR | `gpio.toggle` | in the main loop after the wake | same |

Added: a serial line per wake, and SysTick read across each sleep. SysTick runs from the CPU clock, so a count near a full second of ticks means the clock kept running and a small count means it stopped. The first 5 ticks use SLEEP as the control, the later ones STANDBY0.

## Expected and measured

| Question | Expected | LP-MSPM0L1306 | LP-AM13E230 |
|---|---|---|---|
| Period 1.00 s within 3 percent | yes | 0.9974 s mean over 29 ticks in 28.9 s (host timestamps, sd 0.05 s) | not reached |
| SysTick across a 1 s SLEEP | about 15.2 million (1 s at 32 MHz, modulo 2^24) | 15.17 million (mean of 5) | 15.53 to 15.76 million (1 s at 200 MHz is 15.45 million; 5 ticks) |
| SysTick across a 1 s STANDBY0 | under 100 thousand | **33.8 thousand** (mean of 25, 33,672 to 34,056) | no wake; no ticks after tick 5 |
| STANDBY0 reached with a debugger attached | not expected to matter | **the first run read 15.1 million**: with the probe's debug power request set the chip ran as in SLEEP; after clearing it the numbers above | not applicable |

## The AM13 lock-out

After tick 5 the image entered STANDBY0. Nothing woke it, and every later DSLite connect failed (errors -6310, -6311, -613), as did OpenOCD (-613). Tried and failed: `xds110reset` toggle and assert (the target never printed again), a `uhubctl` power-off of the probe's USB port for 2 s, 20 s and 150 s (the probe came back; the target was not reset, as no serial output followed), connecting within 3 s of the probe appearing, and a hand-written OpenOCD connection. The SDK's low-power example uses the watchdog as the STOP wake source and a GPIO as the STANDBY wake source; this port used the watchdog for STANDBY0. Needs: pull the USB cable and replug, then flash within the awake window, or press the wake button if a GPIO wake is in the running image (the running image has none).

## Size

L1306: 2.1 kB flash, 260 B RAM. AM13: 8.7 kB in bank 0 for the STOP variant, 8.8 kB for the STANDBY0 variant that was flashed. Original: 122 B of FRAM.
