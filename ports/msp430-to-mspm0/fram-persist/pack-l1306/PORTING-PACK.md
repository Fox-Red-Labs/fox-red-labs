# Porting pack: original to LP-MSPM0L1306

Contents: what to change (hardest first) and the check.

Terms: [docs/vocab.md](../../../../docs/vocab.md) and the list in the [folder README](../../README.md). 1 KB = 1,024 bytes.

Read this before you port. It lists only what this project needs: the source calls found, the tested replacement for the target, the traps, and the check. Every entry came from a port run on a real board; `proven` means a run or measurement, `built` means it compiled and was not run, `reported` means a vendor document and no run (treat it as a lead, not a result).

Source: family `plain-c`, 159 lines, APIs found: msp430-driverlib, msp430-regs. Target family: MSPM0. 29 of 29 distinct calls have an entry (100%); 0 do not.

## What to change, hardest first

### vendor.no_equivalent: a call that configures something only the source chip has
Your calls (1 sites): `RTCCTL`
- Note: no tested replacement
- [none, reported, from-msp430] RTC code has no tested replacement here. SLAAE74B Table 3-1 lists an RTC as supported on MSPM0C, MSPM0L and MSPM0G, while this repo's board profile lists RTC missing for the LP-MSPM0L1306; which is right is not settled. Read the part's data sheet first. Instead: a timer from LFCLK for elapsed time
  - Trap: Do not stub RTC registers.
  - Reported only: SLAAE74B, Migration Guide From MSP430 MCUs to MSPM0 MCUs, Rev. B, June 2025, Table 3-1 (RTC row)

### gpio.config_pin: set a pin as output, input, pull-up or pull-down, or alternate function
Your calls (8 sites): `P1DIR`, `P1REN`, `P2DIR`, `P3DIR`, `P2REN`, `P3REN`
- Note: direction, pull enable and function select are three registers on the source and one SysConfig block on the target
- [redesign, proven] A SysConfig GPIO instance per group: direction, pull, interrupt enable and pin assignment. Code only reads, writes and toggles.
  - Trap: Pins are chosen by the migration, not translated: the first UART pins it chose were invalid for the package, and the ADC input moved from PA4 to PA27.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: ports 01 to 05 configure pins in .syscfg

### memory.persistent_variable: a variable that keeps its value across reset and power-off (FRAM-resident, PERSISTENT or NOINIT in the source)
Your calls (2 sites): `PERSISTENT`, `__persistent`
- Note: FRAM parts keep the value: no initializer runs at reset
- [redesign, proven, from-msp430] No FRAM, so no variable keeps its value by being declared persistent. Reserve one flash erase sector outside the linker's FLASH region (cut it from the FLASH length in the linker file) and read it through a const pointer. Write through DL_FlashCTL: 64-bit words with generated ECC, one program per word until the sector is erased. For a handful of bytes that must survive SHUTDOWN but not a power loss, the SHUTDOWN storage bytes (DL_SYSCTL_setShutdownStorageByte) need no flash write.
  - Trap: Rewriting one flash word on every event does not work: a word takes one program after an erase. Flash is rated 100k cycles in the lower 32 KB and 10k above; FRAM is rated 10^15 (SLAAE74B Tables 3-6 and 3-7). Use the append-log idiom below. Code must not execute from the sector being erased, and interrupts stay off during the command.
  - Cost: a write is a flash program, tens of microseconds, and an erase is milliseconds, where an FRAM store is one memory write
  - Proof (measured): msp430-to-mspm0 on mspm0l1306: count kept across 17 software resets, a program load and a 5 s power-off (results `mspm0l1306:persist_survives_reset`)
  - Proof (ran): msp430-to-mspm0 on mspm0l1306: count 15 before and after a 5 s power-off (results `mspm0l1306:persist_survives_power_off`)

### memory.nvm_write_unprotect: open and close the write protection of the program non-volatile memory around a write
Your calls (7 sites): `SYSCFG0`, `FRWPPW`, `DFWP`, `PFWP`
- Note: FRAM write protection for program and data FRAM
- [adapter, reported, from-msp430] Open the sector before each command: DL_FlashCTL_unprotectSector(FLASHCTL, addr, DL_FLASHCTL_REGION_SELECT_MAIN) after DL_FlashCTL_executeClearStatus. Protection is per sector and the unprotect lasts for one command; there is no global write-enable bit like SYSCFG0.
  - Trap: Forgetting the unprotect call makes the command fail with a status, not a fault. Check the returned DL_FLASHCTL_COMMAND_STATUS.
  - Reported only: SLAAE74B, Migration Guide From MSP430 MCUs to MSPM0 MCUs, Rev. B, June 2025, Table 3-6 (protection supported on both); MSPM0 SDK 2.11.00.07 examples/nortos/LP_MSPM0L1306/driverlib/flashctl_multiple_size_write

### power.shutdown_pin_wake: lowest power state: RAM and registers are lost, a pin or reset wakes the chip into a fresh start
Your calls (4 sites): `PMMCTL0_H`, `PMMCTL0_L`, `PMMREGOFF`
- Note: PMMREGOFF with LPM4 gives LPM4.5
- [adapter, reported, from-msp430] LPM4.5 becomes SHUTDOWN: configure a wake pin (DL_GPIO_initDigitalInputFeatures with DL_GPIO_WAKEUP_ON_0), DL_SYSCTL_setPowerPolicySHUTDOWN(), __WFI(). On wake the chip resets; DL_SYSCTL_getResetCause() returns DL_SYSCTL_RESET_CAUSE_BOR_WAKE_FROM_SHUTDOWN, replacing SYSRSTIV_LPM5WU. Call DL_SYSCTL_releaseShutdownIO() after the pins are configured. Four bytes survive in SYSCTL SHUTDNSTORE.
  - Trap: An image that enters SHUTDOWN right after boot cannot be reached by a debugger; wait several seconds awake before sleeping so a probe can attach after every reset, or the board needs the S1 plus reset recovery of mspm0l1306/AGENTS.md.
  - Reported only: SLAAE74B, Migration Guide From MSP430 MCUs to MSPM0 MCUs, Rev. B, June 2025, Table 3-5 (LPM4.5 to SHUTDOWN); MSPM0 SDK 2.11.00.07 examples/nortos/LP_MSPM0L1306/driverlib/sysctl_shutdown

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
- [adapter, proven] Sleep: DL_SYSCTL_setPowerPolicyRUN0SLEEP0() then __WFI(). Deep sleep: DL_SYSCTL_setPowerPolicySTOP0() then __WFI().
  - Proof (ran): cmsemicon on mspm0l1306: STB sleep wakes on the timer; deepsleep_itWakeup woken from STOP0 by the 32 kHz timer
  - Glue you can reuse: `ports/cms32/glue/l1306/CMS32F04xx.h`

### wdt.watchdog: reset the chip if the program stops feeding the watchdog
Your calls (1 sites): `WDTCTL`
- Note: WDTPW|WDTHOLD stops it; WDT_ADLY_* makes an interval timer
- [adapter, proven] WWDT0 in plain watchdog mode (the L1306 has one watchdog; the source chip may have two). If window mode is used, open the window fully in the glue.
  - Trap: Refreshing a window watchdog inside its closed window reset the chip every few ms from the first boot, and every debug connect then failed (OpenOCD: Could not find MEM-AP; DSLite: Connection to MSPM0 core failed). Recovery: mass erase through the debug mailbox with OpenOCD (mspm0x mass erase script).
  - Proof (ran): cmsemicon on mspm0l1306: iwdg ran on WWDT0: unfed, the chip rebooted
  - Glue you can reuse: `ports/cms32/glue/l1306/iwdg_demo.c`

### timer.interval_watchdog_as_timer: use the watchdog in interval mode as a periodic timer that runs in deep sleep
Your calls (1 sites): `WDTCTL`
- Note: WDTPW|WDTHOLD stops it; WDT_ADLY_* makes an interval timer
- [adapter, proven, from-msp430] WWDT0 in interval mode from LFCLK (SysConfig WWDT module, mode INTERVAL, clock divider 1, period 15 bits for 1 s). The interrupt is in interrupt group 0: handle DL_INTERRUPT_GROUP0_IIDX_WWDT0 and read DL_WWDT_getPendingInterrupt(WWDT0).
  - Trap: SysConfig's default is divider 4 and 12 bits (0.5 s); set both. WWDT0 runs in STANDBY0.
  - Proof (measured): msp430-to-mspm0 on mspm0l1306: 29 ticks, mean period 0.9974 s (results `mspm0l1306:deep_sleep_wake_period_ms`)

### gpio.release_after_lowpower_reset: release the pins held by the low-power reset state after they are configured
Your calls (1 sites): `PM5CTL0`
- Note: clearing LOCKLPM5 releases the pins after reset and after LPMx.5
- [adapter, reported, from-msp430] After SHUTDOWN wake call SYSCFG_DL_GPIO_init() and then DL_SYSCTL_releaseShutdownIO(); after a normal reset nothing is held, so the PM5CTL0 LOCKLPM5 clear is deleted.
  - Trap: Release the pins only after they are configured, or they glitch to the reset default.
  - Reported only: MSPM0 SDK 2.11.00.07 examples/nortos/LP_MSPM0L1306/driverlib/sysctl_shutdown

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
- [direct, proven] NVIC_SystemReset is the same CMSIS call; read the cause with DL_SYSCTL_getResetCause (the glue maps it onto Cmsemicon's RESF bit positions).
  - Proof (ran): cmsemicon on mspm0l1306: softReset: RESF 0x00 on the first boot, 0x04 on every boot after
  - Glue you can reuse: `ports/cms32/glue/l1306/cms_core.c`

## The check, on both boards

Run the same check on the source board and the target and keep both numbers (docs/ports.md, method). Record rows in `data/results/<board>.csv`. A port that builds is not a port that passes.

- vendor.no_equivalent: behavior that the call served is checked by name
- gpio.config_pin: read the pin level with and without a load
- memory.persistent_variable: a counter read after a software reset and after a USB power cycle equals the last value written
- memory.nvm_write_unprotect: a program call without the unprotect fails; with it the data reads back
- power.shutdown_pin_wake: reset cause reads wake from shutdown; RAM is cleared; the flash counter is kept
- gpio.input_irq_edge: drive or press the button, one handler run per press
- power.sleep: the wake interrupt runs; measure current for the saving
- wdt.watchdog: unfed watchdog resets, a fed one does not; keep a recovery route before running
- timer.interval_watchdog_as_timer: one interrupt per second for 30 s, no reset
- gpio.release_after_lowpower_reset: pin state after wake equals the state set before sleep
- gpio.write: pin level on a meter or logic analyzer
- time.delay_cycles: pin pulse width at the clock in use
- system.reset: cause flag differs after a software reset from after power-up
