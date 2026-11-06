# Porting pack: original to LP-AM13E230

Contents: what to change (hardest first) and the check.

Terms: [docs/vocab.md](../../../../docs/vocab.md) and the list in the [folder README](../../README.md). 1 KB = 1,024 bytes.

Read this before you port. It lists only what this project needs: the source calls found, the tested replacement for the target, the traps, and the check. Every entry came from a port run on a real board; `proven` means a run or measurement, `built` means it compiled and was not run, `reported` means a vendor document and no run (treat it as a lead, not a result).

Source: family `plain-c`, 159 lines, APIs found: msp430-driverlib, msp430-regs. Target family: AM13. 29 of 29 distinct calls have an entry (100%); 0 do not.

## What to change, hardest first

### vendor.no_equivalent: a call that configures something only the source chip has
Your calls (1 sites): `RTCCTL`
- Note: no tested replacement
- [none, reported, from-msp430] RTC code has no tested replacement here; this repo's profile for the LP-AM13E230 lists RTC missing. Instead: a timer for elapsed time
  - Trap: Do not stub RTC registers.
  - Reported only: portability/everywhere/profiles.json (hw_missing)

### gpio.config_pin: set a pin as output, input, pull-up or pull-down, or alternate function
Your calls (8 sites): `P1DIR`, `P1REN`, `P2DIR`, `P3DIR`, `P2REN`, `P3REN`
- Note: direction, pull enable and function select are three registers on the source and one SysConfig block on the target
- [redesign, proven] A SysConfig GPIO instance per group: direction, pull, interrupt enable and pin assignment. Code only reads, writes and toggles.
  - Trap: Pins are chosen by the migration, not translated: the first UART pins it chose were invalid for the package, and the ADC input moved from PA4 to PA27.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: ports 01 to 05 configure pins in .syscfg

### memory.persistent_variable: a variable that keeps its value across reset and power-off (FRAM-resident, PERSISTENT or NOINIT in the source)
Your calls (2 sites): `PERSISTENT`, `__persistent`
- Note: FRAM parts keep the value: no initializer runs at reset
- [redesign, reported, from-msp430] No FRAM. Use the last sector (2 kB) of flash bank 1; the program lives in bank 0. DL_Flash_program(addr, buf, bytes) and DL_Flash_eraseSector(addr), return DL_FLASH_SUCCESS.
  - Trap: Never erase or program the bank the program runs from (the SDK flash example says so). A tool that loads the program may erase whole banks: a load that erases only bank 0 leaves the data in bank 1 alone.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/flash (main.c, banks); SLAAE74B, Migration Guide From MSP430 MCUs to MSPM0 MCUs, Rev. B, June 2025, Tables 3-6 and 3-7 for the MSP430 side

### power.shutdown_pin_wake: lowest power state: RAM and registers are lost, a pin or reset wakes the chip into a fresh start
Your calls (4 sites): `PMMCTL0_H`, `PMMCTL0_L`, `PMMREGOFF`
- Note: PMMREGOFF with LPM4 gives LPM4.5
- [adapter, reported, from-msp430] DL_SYSCTL_setPowerPolicySHUTDOWN(); reset cause DL_SYSCTL_RESET_CAUSE_BORWAKESHUTDN on wake.
  - Trap: As on MSPM0, an image that sleeps at once cannot be debugged; stay awake for seconds first.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/sysctl/sysctl_low_power_modes; dl_sysctl.h reset cause names

### gpio.input_irq_edge: interrupt on a pin edge, with the pull and edge the button needs
Your calls (3 sites): `P1IES`, `P1IFG`, `P1IE`
- [adapter, proven] SysConfig input pin with the pull and edge the button needs and interrupt enable on; the handler is the GROUP1 read above.
  - Trap: The migration copied ST's active-low setup (PULL_UP, FALL) onto S1, which is active high: the LED toggled for an active-low press only. Read the button's idle level on the target board and set pull and edge from it.
  - Cost: presses were simulated, nobody pressed the button
  - TI's migration guide is silent here: stm32-to-mspm0/peripherals/GPIO.md warns about LED polarity only; it does not say the button's pull and edge must be re-derived
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: with PULL_DOWN and RISE on PA18, each active-high press toggled LED1; presses were driven by the debugger

### power.sleep: stop the CPU until an interrupt, in a sleep or deep sleep state
Your calls (2 sites): `__bis_SR_register`, `LPM4_bits`
- Note: LPM bits plus GIE in one write; target splits the power policy from WFI
- Note: LPM3 and LPM4 are STANDBY or STOP; LPM3.5 and LPM4.5 are SHUTDOWN
- [adapter, reported, from-msp430] DL_SYSCTL_setPowerPolicyRUNSLEEP() then __WFI() for sleep; DL_SYSCTL_setPowerPolicySTOP() for deep sleep with PD0 peripherals running; STANDBY0 or STANDBY1 for the lowest states.
  - Trap: Tested 2026-10-21: a program that entered STANDBY0 with only the watchdog interval timer as a wake source never woke, and the debugger could not connect afterwards (DSLite error -613, OpenOCD -613); a power-cycle of the probe's USB port, xds110reset and connect-under-reset did not recover it. Give STANDBY a GPIO wake pin, and keep a way back to a debuggable state (a delay before sleeping, a button to hold).
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/sysctl/sysctl_low_power_modes (README_AM13E230X.md)

### wdt.watchdog: reset the chip if the program stops feeding the watchdog
Your calls (1 sites): `WDTCTL`
- Note: WDTPW|WDTHOLD stops it; WDT_ADLY_* makes an interval timer
- [adapter, reported, from-msp430] WWDT through the SysConfig watchdog module in watchdog mode.
  - Trap: Window mode: open the window fully when porting a plain MSP430 watchdog.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/wwdt/wwdt_window_mode

### timer.interval_watchdog_as_timer: use the watchdog in interval mode as a periodic timer that runs in deep sleep
Your calls (1 sites): `WDTCTL`
- Note: WDTPW|WDTHOLD stops it; WDT_ADLY_* makes an interval timer
- [adapter, reported, from-msp430] SysConfig WWDT in INTERVAL mode, sleepMode true, interrupt registered (registerInterrupt); the handler is named by SysConfig (APP_WWDT_0_INT_Handler for an instance named APP_WWDT_0). 15-bit period with divider 1 is 1 s.
  - Trap: Divider 2 with 15 bits is 2 s (the SDK example); set divider 1 for 1 s.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/wwdt/wwdt_interval_timer_mode

### gpio.release_after_lowpower_reset: release the pins held by the low-power reset state after they are configured
Your calls (1 sites): `PM5CTL0`
- Note: clearing LOCKLPM5 releases the pins after reset and after LPMx.5
- [adapter, reported, from-msp430] Nothing after a normal reset. After SHUTDOWN wake the SDK sysctl example re-initialises the pins; follow it.
  - Trap: Re-run SYSCFG_DL_init after SHUTDOWN wake.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/sysctl/sysctl_low_power_modes

### memory.nvm_write_unprotect: open and close the write protection of the program non-volatile memory around a write
Your calls (7 sites): `SYSCFG0`, `FRWPPW`, `DFWP`, `PFWP`
- Note: FRAM write protection for program and data FRAM
- [direct, reported, from-msp430] Nothing to open: DL_Flash_program and DL_Flash_eraseSector take the address and do the unprotect. Delete the SYSCFG0 writes.
  - Trap: Flash protection registers (FlashEraseProtectionA and B) can lock sectors; check them if a call fails.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/flash

### gpio.write: drive an output pin high or low
Your calls (6 sites): `P1OUT`, `P2OUT`, `P3OUT`
- Note: also toggled with ^= for gpio.toggle
- [direct, proven] DL_GPIO_setPins(port, mask) and DL_GPIO_clearPins(port, mask).
  - Trap: The LaunchPad LEDs are active low: clearPins turns the LED on. Check the board's AGENTS.md before porting the polarity.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: LED1 toggles every 98.5 ms
  - Proof (ran): cmsemicon on mspm0l1306: PORT_SetBit and PORT_ClrBit on LED1

### time.delay_cycles: blocking wait of N CPU cycles
Your calls (2 sites): `__delay_cycles`
- [direct, proven] DL_Common_delayCycles. On the AM13E230 the one-line header maps the SysConfig macro delay_cycles to it.
  - Proof (ran): mspm0-on-am13 on am13e230: gpio, uart and pwm programs built unchanged against a 13-line stand-in header (`portability/shim/mspm0_on_am13e230/ti_msp_dl_config.h`)
  - Glue you can reuse: `portability/shim/mspm0_on_am13e230/ti_msp_dl_config.h`

### system.reset: software reset and the reset-cause flags read after boot
Your calls (2 sites): `SYSRSTIV`, `SYSRSTIV_LPM5WU`
- Note: reset-cause vector; SYSRSTIV_LPM5WU is the wake from LPMx.5
- [direct, reported, from-msp430] NVIC_SystemReset(); DL_SYSCTL_getResetCause() returns the AM13 names (PORHWFAIL, POREXNRST, BORWAKESHUTDN, CPUSW, SYSDBG and others).
  - Trap: The enum names differ from the MSPM0 ones (BOR_WAKE_FROM_SHUTDOWN becomes BORWAKESHUTDN); the shim header does not map them.
  - Reported only: AM13E2x SDK source/driverlib/am13e230x/dl_sysctl.h

## The check, on both boards

Run the same check on the source board and the target and keep both numbers (docs/ports.md, method). Record rows in `data/results/<board>.csv`. A port that builds is not a port that passes.

- vendor.no_equivalent: behavior that the call served is checked by name
- gpio.config_pin: read the pin level with and without a load
- memory.persistent_variable: a counter read after a software reset equals the last value written
- power.shutdown_pin_wake: reset cause reads wake from shutdown
- gpio.input_irq_edge: drive or press the button, one handler run per press
- power.sleep: the wake interrupt runs; the debugger still connects after a reset
- wdt.watchdog: unfed watchdog resets; fed one does not
- timer.interval_watchdog_as_timer: one interrupt per second, no reset
- gpio.release_after_lowpower_reset: pin state after wake equals the state set before sleep
- memory.nvm_write_unprotect: program then read back
- gpio.write: pin level on a meter or logic analyzer
- time.delay_cycles: pin pulse width at the clock in use
- system.reset: cause differs after a software reset from after power-up
