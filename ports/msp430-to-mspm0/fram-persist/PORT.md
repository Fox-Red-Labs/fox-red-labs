# Moving TI's MSP430FR2433 persistent-variable example to the LP-MSPM0L1306 and the LP-AM13E230

The variable that lived in FRAM became an append-only log in one flash sector, the low-power mode became STANDBY0 with a button interrupt, and the register writes became a SysConfig file. The count survived 17 software resets in a row, a new program load, and a 5 s USB power-off on the L1306. The AM13 build was never run because the board stopped accepting a debugger (see [lpm3-wake](../lpm3-wake/PORT.md)).

Terms: [docs/vocab.md](../../../docs/vocab.md), and the term list in the [folder README](../README.md). 1 kB = 1,024 bytes.

Source: `msp430fr243x_framwrite_persistent.c` from TI's MSP430Ware code examples for the MSP430FR243x family, Ling Zhu, August 2015, BSD-3-Clause (the notice stays in `original/`). Read from a GitHub copy of MSP430Ware (`PiBoxY/msp430ware`, commit 7c96db0); TI's own download was not used. Original: LED on P1.0, button S1 on P1.3, wakes from LPM4.5, adds one to a `#pragma PERSISTENT` variable in FRAM, and blinks forever once the count reaches 5. Compiled for the MSP430FR2433: 224 B of FRAM, 128 B of RAM (stack), not run.

## What changed, by idiom

| MSP430 | Idiom | LP-MSPM0L1306 | LP-AM13E230 |
|---|---|---|---|
| `#pragma PERSISTENT(Port_event)` | `memory.persistent_variable`, `storage.append_log_counter` | 128 slots of 8 bytes `(count, ~count)` in the sector at 0xF800, cut out of the linker's flash region; newest valid slot wins; erase when full | 256 slots in the last 2 kB sector of bank 1 (0x7F800); program in bank 0 |
| `SYSCFG0 = FRWPPW \| DFWP` around `Port_event++` | `memory.nvm_write_unprotect` | `DL_FlashCTL_unprotectSector` and `executeClearStatus` before each command, interrupts off | none: `DL_Flash_program` does it |
| `Port_event++` | | `DL_FlashCTL_programMemoryFromRAM64WithECCGenerated` into the next slot | `DL_Flash_program(addr, buf, 8)` |
| `SYSRSTIV == SYSRSTIV_LPM5WU` | `system.reset` | `DL_SYSCTL_getResetCause() == BOR_WAKE_FROM_SHUTDOWN`; the test builds count every reset except power-on (`COUNT_ANY_RESET`) because nobody could press the button | same, enum `BORWAKESHUTDN` |
| `PMMREGOFF` and `LPM4_bits` | `power.shutdown_pin_wake` | default: STANDBY0 and a falling-edge interrupt on S2 (PA14). `-DSLEEP_SHUTDOWN=1`: SHUTDOWN with S2 as wake pin, 5 s awake first. Built, not run | wait loop by default (`SLEEP_MODE` 0). STANDBY0 (1) and SHUTDOWN (2) built, not run |
| `PM5CTL0 &= ~LOCKLPM5` | `gpio.release_after_lowpower_reset` | `DL_SYSCTL_releaseShutdownIO()` in the SHUTDOWN path only | not needed |
| `P1DIR`, `P1REN`, `P1OUT`, `P1IES`, `P1IE`, `P1IFG` | `gpio.config_pin`, `gpio.input_irq_edge` | SysConfig: LED PA0, S2 PA14 pull-up falling edge; handler `GROUP1_IRQHandler` | SysConfig: LED PA5, wake button PB13 |
| `P2DIR = P3DIR = 0xFF` and the other unused-pin lines | `gpio.unused_pins_safe` | `Board.configureUnused = true` | removed |
| `RTCCTL = 0`, LCD lines | none | deleted | deleted |
| `__delay_cycles(100000)` | `time.delay_cycles` | `delay_cycles(3200000)` for 100 ms at 32 MHz | `DEVICE_DELAY_MS(100)` |

Added for the tests: a serial line per boot (`A1 boot cause=.. count=.. slot=..`), a recap line 3 s after boot (so a serial port opened after a power-up still sees the count), `STORM` (software reset until the count reaches N), `BURST` (add N counts and time the flash), `ERASE_AT_BOOT`.

## Expected and measured (LP-MSPM0L1306)

| Question | Expected before the run | Measured |
|---|---|---|
| Count survives a software reset | +1 per reset | +1 per reset for 17 resets in a row (counts 4 to 20, slot = count minus 1) |
| Count survives a new program load | continues | continued (1, then 3 after the next load, which boots more than once) |
| Count survives power-off | kept | kept: 15 before a 5 s USB power-off, 15 after (the power-up boot is not counted); the chip had restarted |
| Word write | 50 to 200 µs | 56 µs (mean and max of 300) |
| Sector erase | 5 to 40 ms | **1.99 ms**, below the expected range |
| Erases in a 300-count burst | 2 | 2; final count 301; slot 0 read back as 0x101 |
| LED blinks at 5 | yes | message `blinking forever` at count 5; GPIOA DOUT bit 0 changed in 1 of 5 reads |

Counts that jump by two or three per probe reset are OpenOCD resetting the chip on connect; the software-reset storm has no probe in the loop and counts exactly.

## Not tested

A real button press (wake from STANDBY0 or SHUTDOWN); power-off persistence on the AM13; current in any state; endurance beyond a few hundred sector erases; the AM13 run.

## Size

LP-MSPM0L1306: 4.6 kB of flash, 365 B of RAM including the stack (TI Arm Clang -O2). LP-AM13E230: 12.9 kB in flash bank 0. Original: 224 B of FRAM.
