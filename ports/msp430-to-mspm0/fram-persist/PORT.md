# Moving TI's MSP430FR2433 persistent-variable example to the LP-MSPM0L1306, the LP-AM13E230, the LP-MSPM33C321A and the LAUNCHXL-F2800137

The variable that lived in FRAM became an append-only log in one flash sector, the low-power mode became STANDBY0 with a button interrupt, and the register writes became a SysConfig file. The count survived 17 software resets in a row, a new program load, and a 5 s USB power-off on the L1306. The AM13 build first failed to run because the board stopped accepting a debugger (see [lpm3-wake](../lpm3-wake/PORT.md)) and ran later with 16-byte slots. The LP-MSPM33C321A needed 16-byte slots too (one 128-bit flash word), counted 19 software resets without a gap and kept its count through a program load that erases only the image's sectors. The LAUNCHXL-F2800137 (a C2000 chip with 16-bit bytes) took a 64-bit slot, counted 19 software resets and kept its count only through a load that erases the sectors the image uses, not through the debugger's default load, which erases all of flash.

Terms: [docs/vocab.md](../../../docs/vocab.md), and the term list in the [folder README](../README.md). 1 kB = 1,024 bytes.

Source: `msp430fr243x_framwrite_persistent.c` from TI's MSP430Ware code examples for the MSP430FR243x family, Ling Zhu, August 2015, BSD-3-Clause (the notice stays in `original/`). Read from a GitHub copy of MSP430Ware (`PiBoxY/msp430ware`, commit 7c96db0); TI's own download was not used. Original: LED on P1.0, button S1 on P1.3, wakes from LPM4.5, adds one to a `#pragma PERSISTENT` variable in FRAM, and blinks forever once the count reaches 5. Compiled for the MSP430FR2433: 224 B of FRAM, 128 B of RAM (stack), not run.

## What changed, by idiom

| MSP430 | Idiom | LP-MSPM0L1306 | LP-AM13E230 | LP-MSPM33C321A | LAUNCHXL-F2800137 |
|---|---|---|---|---|---|
| `#pragma PERSISTENT(Port_event)` | `memory.persistent_variable`, `storage.append_log_counter` | 128 slots of 8 bytes `(count, ~count)` in the sector at 0xF800, cut out of the linker's flash region; newest valid slot wins; erase when full | 256 slots in the last 2 kB sector of bank 1 (0x7F800); program in bank 0 | 128 slots of 16 bytes in the last 2 kB sector of main flash (CPU address 0x100FF800, controller address 0xFF800), cut out of the linker's FLASH region; program in bank 0 | 256 slots of 4 words (64 bits) `(count, ~count)` in sector 100 (0x99000), which the image and the shipped demo do not use; the image sits in sectors 0, 8, 32 and 40 |
| `SYSCFG0 = FRWPPW \| DFWP` around `Port_event++` | `memory.nvm_write_unprotect` | `DL_FlashCTL_unprotectSector` and `executeClearStatus` before each command, interrupts off | none: `DL_Flash_program` does it | `GSC` flash semaphore once, then `executeClearStatus` and `unprotectSector` (region MAIN) before each command | `Fapi_setupBankSectorEnable` writes `CMDWEPROTA` and `CMDWEPROTB` before each command (a set bit protects: every group of 8 sectors but 96 to 103 stays protected), after `Fapi_ClearStatus`; interrupts off |
| `Port_event++` | | `DL_FlashCTL_programMemoryFromRAM64WithECCGenerated` into the next slot | `DL_Flash_program(addr, buf, 8)` | `DL_FlashCTL_programMemoryFromRAM128WithECCGenerated` into the next slot | `Fapi_issueProgrammingCommand(addr, buf, 4, 0, 0, Fapi_AutoEccGeneration)`, then `Fapi_doVerify`; the erase is `Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, addr)`. All of it runs from RAM (`.TI.ramfunc`) |
| `SYSRSTIV == SYSRSTIV_LPM5WU` | `system.reset` | `DL_SYSCTL_getResetCause() == BOR_WAKE_FROM_SHUTDOWN`; the test builds count every reset except power-on (`COUNT_ANY_RESET`) because nobody could press the button | same, enum `BORWAKESHUTDN` | same, enum `BOR_WAKE_FROM_SHUTDOWN`; the test build counts every reset except power-on | `SysCtl_getResetCause()` tested against `SYSCTL_CAUSE_POR`, then `SysCtl_clearResetCause()`; the test builds count every reset except power-on; the software reset is `SysCtl_resetDevice()`, a watchdog reset |
| `PMMREGOFF` and `LPM4_bits` | `power.shutdown_pin_wake` | default: STANDBY0 and a falling-edge interrupt on S2 (PA14). `-DSLEEP_SHUTDOWN=1`: SHUTDOWN with S2 as wake pin, 5 s awake first. Built, not run | wait loop by default (`SLEEP_MODE` 0). STANDBY0 (1) and SHUTDOWN (2) built, not run | wait loop only; no sleep mode, no wake button | wait loop; no sleep mode built |
| `PM5CTL0 &= ~LOCKLPM5` | `gpio.release_after_lowpower_reset` | `DL_SYSCTL_releaseShutdownIO()` in the SHUTDOWN path only | not needed | not needed | not needed |
| `P1DIR`, `P1REN`, `P1OUT`, `P1IES`, `P1IE`, `P1IFG` | `gpio.config_pin`, `gpio.input_irq_edge` | SysConfig: LED PA0, S2 PA14 pull-up falling edge; handler `GROUP1_IRQHandler` | SysConfig: LED PA5, wake button PB13 | SysConfig: LED2 green PC27 (red PC26), serial PA10 and PA11; no button | driverlib calls in `main.c` (no SysConfig): LED4 on GPIO20, serial log on SCI-A (GPIO28 and 29, 115200 baud); no button |
| `P2DIR = P3DIR = 0xFF` and the other unused-pin lines | `gpio.unused_pins_safe` | `Board.configureUnused = true` | removed | removed | left at the reset defaults |
| `RTCCTL = 0`, LCD lines | none | deleted | deleted | deleted | deleted |
| `__delay_cycles(100000)` | `time.delay_cycles` | `delay_cycles(3200000)` for 100 ms at 32 MHz | `DEVICE_DELAY_MS(100)` | `delay_cycles(3200000)` for 100 ms at 32 MHz | `DEVICE_DELAY_US(100000)` |

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

## LP-AM13E230 run (2026-11-08)

Built with `./build_am13.sh fram-persist`, loaded with DSLite, default wait-loop build (no sleep). The L1306's 8-byte slot failed here: `DL_Flash_program` returns `DL_FLASH_ADDR_INVALID_ALIGN` for any address that is not 16-byte aligned (`dl_flash.c`; the header says 8-byte), so slot 0 wrote and every later slot returned -1, and the count stuck at 2 across resets (`results/am13-storm-8byte-slots.log`). With 16-byte slots (count, inverted count, two blank words) the log works.

| Question | Measured on the LP-AM13E230 |
|---|---|
| Count survives a software reset | +1 per reset from 2 to 8 (storm build), then 9 after a probe reset; slot = count minus 1 (`results/am13-storm-16byte-slots.log`, `...-end.log`) |
| Count survives a new program load | continued across three loads |
| Word write | 70 µs mean, 71 µs max (300 slots of 16 bytes) |
| Sector erase | 2.79 ms (2 KB sector; 2 erases in the 300-count burst, final count 310, slot 53) |
| LED blinks at 5 | message `blinking forever` at count 9 in the storm run; the pin was not read |

## LP-MSPM33C321A run (2026-11-08)

Built with `./build_m33.sh fram-persist` (SysConfig, TI Arm Clang 5.1.1, MSPM33 SDK 1.04.00.00, 32 MHz default clock), loaded with DSLite, wait-loop build only. The flash code is `DL_FlashCTL_*`, the same family as the MSPM0 parts, with three differences found in the driver source before the first load: the flash word is 128 bits (`FLASHCTL_SYS_DATAWIDTH` in `mspm33c321x.h`; the header asks for a flash word aligned address even for an 8-bit program), the sector is 2 kB (`DL_FLASHCTL_SECTOR_SIZE`), and the flash controller counts main flash from 0 while the CPU reads it at 0x10000000 (log at 0xFF800 and 0x100FF800). So the slot is 16 bytes, as on the AM13. The 8-byte layout was not tried: a bad program of a half word could leave a bad ECC code in a sector the code reads at boot, and a boot fault is the lockup this board's [recovery section](../../../mspm33c321a/AGENTS.md) describes. The log sector is cut out of the linker's FLASH region (`build_m33.sh`), and the map shows no NONMAIN use. Expectations were written before the first load (`results/m33-expected.txt`).

| Question | Expected before the run | Measured |
|---|---|---|
| Count survives a software reset | +1 per reset | +1 per reset for 19 resets in a row, counts 1 to 20, slot = count minus 1 (`results/m33-storm.log`); one probe pin reset added one more (`results/m33-first-load.log`) |
| Count survives a new program load | continues when only the image's sectors are erased, lost on a plain `-e` | both as expected: DSLite with `FlashEraseSelection` set to "by range" took the count from 20 to 21 (`results/m33-load-range.log`); a plain `-e` erased all of main flash and the count restarted at 1 (`results/m33-load-plain-e.log`). Setting only the range start and end without the selection also erased everything |
| Word write | 50 to 150 µs | 80 µs mean, 81 µs max (300 slots of 16 bytes) |
| Sector erase | 2 to 10 ms | 2.59 ms (2 erases in the 300-count burst, final count 301 in slot 44; slot 0 read back as 0x101 over the probe, `results/m33-sector-after-burst.txt`) |
| LED blinks at 5 | yes | message `blinking forever` at count 21 on a reload; the pin was not read |

Reset causes seen on the serial line: 26 after a DSLite load and run, 12 after a pulse of the board's reset line (`xds110reset`), 29 after `NVIC_SystemReset`. The board was backed up first (main flash, the 32 kB data flash and the boot configuration read twice and compared) and put back with the same image afterwards; all three regions read back identical (the backup is outside the repo).

## LAUNCHXL-F2800137 run (2026-11-08)

Built with `./build_f28.sh fram-persist` (TI C2000 compiler 25.11.1.LTS -O2 with `--c99`, C2000Ware 26.02.00.00, Flash API 2.00.10, linker file `280013x_flash_api_lnk.cmd`; no SysConfig, the setup is driverlib calls in `main.c`). Expectations were written before the first load (`results/f28-expectations.txt`). Facts read from C2000Ware before the first load:

| Question | Answer on the F2800137 |
|---|---|
| Byte size | `char`, `short` and `int` are 16 bits and flash addresses count 16-bit words. The shared `log.h` works unchanged once `driverlib.h` (which defines `uint8_t`) is included first |
| Program unit and ECC | 64 bits (4 words), 64-bit aligned, once per erase. The Flash API (FAPI) writes the ECC itself with `Fapi_AutoEccGeneration`, so a slot is `(count, ~count)` as two 32-bit values in 4 words |
| Sector | 1,024 words (2 kB), 128 sectors from 0x80000, one bank. The log is sector 100 |
| Where the code runs | The chip has one bank, so every FAPI call and the functions around it run from RAM: they are in `.TI.ramfunc`, which `Device_init()` copies (1.7k words), and interrupts are off during a command |
| Reset cause | `SysCtl_getResetCause()`: POR 0x1, XRS 0x2, WDRS 0x4. The bits stay set until cleared, so the boot clears them |
| Write protection | `CMDWEPROTA` (sectors 0 to 31) and `CMDWEPROTB` (groups of 8 sectors from 32): a set bit protects. Everything but sectors 96 to 103 is protected for each command. No OTP, DCSM or security zone is written |

| Question | Expected before the run | Measured |
|---|---|---|
| Reading the blank log at boot | no ECC fault (erased data and ECC are both all ones) | no fault: `fault=0` on the first boot, no NMI (`results/f28-first-load.log`) |
| Count survives a software reset | +1 per reset, cause WDRS (0x4) | +1 per reset for 19 resets in a row, counts 1 to 20, slot = count minus 1, `fault=0` (`results/f28-storm.log`). The cause read **6**, not 4: the watchdog reset also sets XRS (it pulls the reset pin). A probe System Reset reads 6 too; a load by the scripting runner reads 0 |
| Count survives a new program load | continues, because the debugger erases only the sectors the image uses | **depends on the loader**. DSLite with `FlashEraseSelection=1` ("Necessary Sectors Only") kept it: slot 22 holds count 23 after the load, so slots 0 to 21 survived (`results/f28-reload-dslite.log`). CCStudio's scripting runner, and DSLite's default "Entire Flash", erased the log: the count went from 23 to 1 (`results/f28-reload-ccs-scripting.log`) |
| Word write (4 words, AutoEcc, wait for the flash state machine, verify) | 40 to 150 µs, guess 70 | 64 µs mean and max of 300, the same in three bursts (`results/f28-burst*.log`) |
| Sector erase (2 kB, with a blank check) | 3 to 60 ms, guess 15 | **2.18 ms**, below the expected range, as on the L1306 and the AM13. The timer was checked against a 100 ms delay loop, which read 101.0 ms |
| Erases in a 300-count burst | 1 or 2 | 1 each time (counts 2 to 302 ending in slot 45, 303 to 603 in slot 90, 604 to 904 in slot 135) |
| LED blinks at 5 | message at count 5, GPIO20 toggles | message `blinking forever` at count 5 after four probe resets (`results/f28-blink-at-5.log`); GPIO20 read 0 in 13 and 1 in 11 of 24 samples 70 ms apart (`results/f28-blink-pin.txt`) |

The load by the scripting runner counts as a boot (cause 0); the DSLite load counts one or two. The first bytes on the serial port after a load are sometimes junk from the probe, never a log line.

The board was backed up first (main flash read twice, SHA-256 bf2e8054...), put back with `c2kflashback.js` and `c2krestore.js` afterwards, and the readback matched the backup byte for byte; the demo prints `ADCINA6 Sample` again. Probe actions go through `tools/harness_f28.py` and `tools/f28probe.js` (set `F28_LOADER=dslite` for the loads that keep the log).

## Not tested

A real button press (wake from STANDBY0 or SHUTDOWN); the button and any sleep mode on the LP-MSPM33C321A (the build is a wait loop); power-off persistence on the AM13 (a USB port power-off does not reset that board), on the LP-MSPM33C321A and on the LAUNCHXL-F2800137 (no switchable hub in these runs); a button and any sleep or wake mode on the F2800137 (the build is a wait loop); the 128-bit program length of the Flash API (only 64 bits were used); the 8-byte slot layout on the LP-MSPM33C321A; current in any state; endurance beyond a few hundred sector erases.

## Size

LP-MSPM0L1306: 4.6 kB of flash, 365 B of RAM including the stack (TI Arm Clang -O2). LP-AM13E230: 12.9 kB in flash bank 0. LP-MSPM33C321A: 4.9 kB of flash, 384 B of RAM including the 256 B stack. LAUNCHXL-F2800137: 8.2 kB of flash (4.2k words: 2,369 of code, 1,666 of Flash API and flash-function code that runs from RAM, 148 of constants) and 4.3 kB of RAM (the 1.7k-word copy of those functions, a 1 kB stack and 32 B of data), TI C2000 compiler -O2. Original: 224 B of FRAM.
