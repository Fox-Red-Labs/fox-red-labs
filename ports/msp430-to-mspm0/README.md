# Porting an MSP430 FRAM project to an MSPM0 or AM13 LaunchPad

Move each FRAM variable into a flash log, turn each low-power mode into the nearest MSPM0 power policy with a wake source you can name, let SysConfig replace the clock and pin set-up, and test the sleep with the debugger disconnected. Three TI MSP430FR2433 code examples were ported this way to the LP-MSPM0L1306 and ran on it: a counter that survives reset and a 5 s USB power-off, a 1 s watchdog interval timer that wakes the chip from deep sleep, and a 9600 baud UART echo that runs from deep sleep. The same three were built for the LP-AM13E230. On the AM13 the first deep-sleep test hung the chip and the debugger could no longer connect, so only that one program ran there.

Terms: [docs/vocab.md](../../docs/vocab.md). On this page: *FRAM* is the MSP430FR parts' non-volatile memory that is written like RAM; *LPM3*, *LPM4.5* are MSP430 low-power modes (clocks off except a 32 kHz one; everything off but non-volatile memory); *STANDBY0*, *STOP*, *SHUTDOWN* are MSPM0 and AM13 power states, from lightest to deepest; *SysConfig* writes a chip's pin and clock set-up code; *WWDT* is the window watchdog, usable as an interval timer; *LFOSC* is the internal 32 kHz oscillator; *SysTick* is the Arm core's own counter, which stops when the CPU clock stops; *DAP* is the debug access port. 1 kB = 1,024 bytes.

Checked 2026-10-21 on an Intel Mac, macOS 15.7.9, with MSPM0 SDK 2.11.00.07, AM13E2x SDK 26.01.00.03.STS, TI Arm Clang 5.1.1.LTS, TI MSP430 compiler 21.6.3.LTS, SysConfig 1.28.0, xPack OpenOCD 0.12.0 and DSLite 21.0.0.4166, on one LP-MSPM0L1306 and one LP-AM13E230. No MSP430 LaunchPad was used, so the originals were compiled for the MSP430FR2433 (sizes below) and never run.

## What ran

| Project (TI MSP430FR243x example) | LP-MSPM0L1306 | LP-AM13E230 | Original, MSP430FR2433 |
|---|---|---|---|
| [fram-persist](fram-persist/PORT.md) (`framwrite_persistent`) | Ran. Count survived 17 software resets in a row, a new program load, and a 5 s power-off. Flash word write 56 µs, 1 kB sector erase 1.99 ms. 4.6 kB flash | Built, not run (board unreachable). 12.9 kB flash | 224 B FRAM, built |
| [lpm3-wake](lpm3-wake/PORT.md) (`LPM3_01`) | Ran. Period 0.997 s over 29 ticks. CPU clock stopped in STANDBY0: 33.8 thousand SysTick ticks per 1 s sleep against 15.2 million in SLEEP. 2.1 kB flash | Ran 5 ticks in SLEEP; STANDBY0 with only the watchdog timer never woke and the debugger could not connect afterwards. 8.8 kB flash | 122 B FRAM, built |
| [uart-echo-lpm3](uart-echo-lpm3/PORT.md) (`euscia0_uart_01`) | Ran. 190 of 190 bytes of a burst echoed at 9600 baud from STANDBY0; a UART on the 32 kHz clock lost most of a burst | Built, not run. 7.8 kB flash | 392 B FRAM, built |

Not measured: current in any state (no meter), wake latency on a pin, a real button press (nobody could press one), and SHUTDOWN (built, never run: see trap 6).

## The ten steps

1. **Scan the project.** List each MSP430 call or register, what it becomes, the trap and the check. The packs for these three projects are in each project folder (`pack-l1306/`, `pack-am13e230/`). A call with no entry is listed as having none.
2. **Check the part.** SLAAE74B Table 3-1 compares the families: MSP430 runs to 25 MHz, MSPM0L to 32 MHz, MSPM0G to 80 MHz, AM13E230 to 200 MHz. MSP430 SPI runs to 8 MHz and I2C to 100 kHz; MSPM0 reaches 16 or 32 MHz SPI and 1 MHz I2C.
3. **Write the SysConfig file.** One block each for pins, the clock, the UART and the timer. Delete the oscillator set-up, the fault-flag loops, the DCO trim routine (SYSOSC is factory-trimmed), the `PM5CTL0 &= ~LOCKLPM5` line and the unused-pin loops (SysConfig `Board.configureUnused`).
4. **Place each FRAM variable.** A value that must survive reset goes in a flash log (step 5). A value that must survive only SHUTDOWN can use the four shutdown storage bytes. Constants stay `const` in flash.
5. **Replace the persistent variable.** Reserve one erase sector outside the linker's flash region. Keep the counter as an append-only log of 8-byte slots, `(count, ~count)`, each programmed once; at boot the newest valid slot is the count; erase only when the sector is full. 128 slots per 1 kB sector on the L1306, 256 per 2 kB sector on the AM13.
6. **Map the low-power modes.** LPM0 is SLEEP; LPM3 is STANDBY0 (or STOP on the AM13) with a wake source that runs there; LPM4.5 is SHUTDOWN with a wake pin and a reset on wake. The MSP430 `SYSRSTIV_LPM5WU` test becomes `DL_SYSCTL_RESET_CAUSE_BOR_WAKE_FROM_SHUTDOWN`.
7. **Map the timer and serial parts.** The watchdog in interval mode becomes WWDT in interval mode. `Timer_A` becomes a TIMG instance. eUSCI_A in UART mode becomes a UART instance, eUSCI_B in I2C or SPI mode a controller instance.
8. **Build.** `./build_l1306.sh <project>` or `./build_am13.sh <project>` (SysConfig, TI Arm Clang, the SDK linker file). `./build_originals.sh` compiles the MSP430 originals.
9. **Flash and reset one board at a time, and log the serial output with time stamps.** Write the expected result before each run, so a surprise stands out.
10. **Test the sleep with the debugger away.** Clear the debug power request, then read a counter that only runs while the CPU clock runs (SysTick) across the sleep. Keep a route back to a debuggable chip before you test a deep state (trap 3).

## Traps

1. **FRAM wears out in no human time; flash does.** FRAM is rated 10^15 cycles; MSPM0 flash 100k in the lower 32 kB and 10k above (SLAAE74B Tables 3-6, 3-7). A counter bumped on every event needs the log in step 5. A power loss between the erase and the first program of a full log loses the count; two sectors used in turn close that window.
2. **A debugger keeps the chip out of STOP and STANDBY.** After OpenOCD disconnects, the debug power request stays set; the L1306 then behaved like SLEEP (15.1 million SysTick ticks per second). `mspm0x.dap dpreg 0x04 0x00000000` clears it (this resets the chip once more). A probe reset through OpenOCD also boots the program two or three times, so a boot counter rises by that much per reset.
3. **Deep sleep can lock the debugger out.** On the LP-AM13E230 an image in STANDBY0 whose only wake source was the watchdog interval timer never woke. Every connect then failed (DSLite -613, -6310, -6311; OpenOCD -613). `xds110reset`, a USB port power-off of 150 s and connect-under-reset did not bring it back. The SDK documents the watchdog as the STOP wake source and a GPIO as the STANDBY wake source. Give every deep-sleep image a documented wake source and a delay of several seconds awake at boot.
4. **A UART on the 32 kHz clock echoes typed bytes, not bursts.** With UART0 on LFCLK at 3x oversampling (the mode that receives in STANDBY1) the L1306 echoed single bytes 0.3 s apart, but kept only the first byte and the last four of a 38-byte burst. The same handler with the UART on the bus clock in STANDBY0 echoed a 190-byte burst unchanged. The cause was not isolated.
5. **Check the FIFO threshold.** SysConfig's default receive threshold is half full (2 of 4 entries). Set one entry so each byte raises the interrupt, and drain the FIFO in the handler.
6. **A chip that sleeps at once cannot be debugged.** The SHUTDOWN build waits 5 s awake before sleeping for that reason. It was built and not run, because a wake needs a button press.
7. **Watchdog defaults are not 1 s.** SysConfig's interval defaults are divider 4 and 12 bits, 0.5 s. Divider 1 and 15 bits give 1 s from 32768 Hz.
8. **Flash tools can erase by bank.** DSLite offers "Necessary Bank(s) Only", "Entire Flash" and per-bank erase scopes on the AM13, and its `-e` flag means verbose output, not erase. Keep program data in the bank the program does not use (bank 1) and do not assume a program load leaves a sector in its own bank alone.
9. **Power-cycle tricks are board-specific.** A `uhubctl` port power-off removed power from the L1306 (its tick count restarted) and did not reset the AM13.

More, with the proof for each: PORTING-LESSONS.md.

## Repeat it

```bash
cd ports/msp430-to-mspm0
./build_originals.sh                                       # the three MSP430 originals, build only
./build_l1306.sh lpm3-wake && ./build_l1306.sh fram-persist && ./build_l1306.sh uart-echo-lpm3
./build_am13.sh lpm3-wake                                  # same for the other two
# then flash the .out with DSLite and read the console at 9600 baud: see the AGENTS.md of the board
```

Other build switches are listed at the top of each `main.c` (`STORM`, `BURST`, `ERASE_AT_BOOT`, `COUNT_ANY_RESET`, `SLEEP_SHUTDOWN` and `SLEEP_MODE`). The originals keep their TI copyright and BSD-3-Clause notice; they are from the `msp430ware` code examples for the MSP430FR243x family.

## Files

`fram-persist/`, `lpm3-wake/`, `uart-echo-lpm3/`: each has `original/` (TI's file as shipped), `l1306/` and `am13e230/` (the ports and their SysConfig files), `pack-l1306/` and `pack-am13e230/` (trick-map packs), `PORT.md`. `data/porting/apis/msp430-driverlib.json` and `msp430-regs.json` hold the MSP430 side of the trick map; `data/porting/impl/mspm0-from-msp430.json` and `am13-from-msp430.json` the replacements, each marked reported until proven.
