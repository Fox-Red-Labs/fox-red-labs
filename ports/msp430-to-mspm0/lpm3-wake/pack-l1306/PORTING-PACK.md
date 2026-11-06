# Porting pack: original to LP-MSPM0L1306

Terms: [docs/vocab.md](../../../../docs/vocab.md) and the list in the [folder README](../../README.md). 1 KB = 1,024 bytes.

Read this before you port. It lists only what this project needs: the source calls found, the tested replacement for the target, the traps, and the check. Every entry came from a port run on a real board; `proven` means a run or measurement, `built` means it compiled and was not run, `reported` means a vendor document and no run (treat it as a lead, not a result).

Source: family `plain-c`, 112 lines, APIs found: msp430-driverlib, msp430-regs. Target family: MSPM0. 19 of 19 distinct calls have an entry (100%); 0 do not.

## What to change, hardest first

### gpio.config_pin: set a pin as output, input, pull-up or pull-down, or alternate function
Your calls (4 sites): `P2SEL0`, `P1DIR`, `P2DIR`, `P3DIR`
- Note: direction, pull enable and function select are three registers on the source and one SysConfig block on the target
- [redesign, proven] A SysConfig GPIO instance per group: direction, pull, interrupt enable and pin assignment. Code only reads, writes and toggles.
  - Trap: Pins are chosen by the migration, not translated: the first UART pins it chose were invalid for the package, and the ADC input moved from PA4 to PA27.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: ports 01 to 05 configure pins in .syscfg

### clock.system_config: choose the CPU and bus clock tree at start
Your calls (4 sites): `CSCTL4`, `SELMS__DCOCLKDIV`, `SELA__XT1CLK`, `CSCTL7`
- [redesign, proven] Set the clock tree in the SysConfig SYSCTL block. The default is 32 MHz from SYSOSC; keep it unless a rate needs more.
  - Trap: STM32 code runs at 48 or 64 MHz from a PLL; the ports ran at 32 MHz, so a timing number compares two different CPU clocks. A 56 MHz setting with the bus divider at 1 printed correctly on the UART, but TI's header says that bus clock is to stay under 40 MHz: unknown whether 56 MHz is in spec.
  - Cost: lower CPU clock than the original
  - Proof (measured): stm32-usart-dma-rx-tx on mspm0g3519: left at the 32 MHz default, 6 of 6 host checks at 115200 baud (results `mspm0g3519:uartdma_burst_pps`)
  - Proof (ran): stm32-bootloader on mspm0g3519: left at the SysConfig default 32 MHz (the original runs from an 8 MHz crystal); the 1 s receive timeout and 115200 baud divisors both come from CPUCLK_FREQ; 6 of 6 host checks (`ports/stm32-bootloader/boards/mspm0g3519/Src/uart.c`; results `mspm0g3519:bootloader_checks_passed`)

### timer.interval_watchdog_as_timer: use the watchdog in interval mode as a periodic timer that runs in deep sleep
Your calls (6 sites): `SFRIFG1`, `WDTCTL`, `WDT_ADLY_1000`, `SFRIE1`, `WDTIE`
- Note: WDTPW|WDTHOLD stops it; WDT_ADLY_* makes an interval timer
- [adapter, proven, from-msp430] WWDT0 in interval mode from LFCLK (SysConfig WWDT module, mode INTERVAL, clock divider 1, period 15 bits for 1 s). The interrupt is in interrupt group 0: handle DL_INTERRUPT_GROUP0_IIDX_WWDT0 and read DL_WWDT_getPendingInterrupt(WWDT0).
  - Trap: SysConfig's default is divider 4 and 12 bits (0.5 s); set both. WWDT0 runs in STANDBY0.
  - Proof (measured): msp430-to-mspm0 on mspm0l1306: 29 ticks, mean period 0.9974 s (results `mspm0l1306:deep_sleep_wake_period_ms`)

### clock.low_freq_aclk: choose the low-frequency clock that keeps running in deep sleep (crystal or internal 32 kHz)
Your calls (4 sites): `CSCTL4`, `SELMS__DCOCLKDIV`, `SELA__XT1CLK`, `CSCTL7`
- [adapter, proven, from-msp430] ACLK is a fixed 32 kHz clock on MSPM0 (LFCLK, from the internal LFOSC or an external LFXT crystal). Peripherals pick LFCLK in SysConfig (timer clock source, UART clock source). Delete the XT1 pin setup and the oscillator fault loop.
  - Trap: LFOSC is not a crystal: the MSP430 example's accuracy comes from XT1. Add the LFXT crystal if the period must be exact.
  - Proof (measured): msp430-to-mspm0 on mspm0l1306: the same period test with the timer on the 32 kHz oscillator (results `mspm0l1306:deep_sleep_wake_period_ms`)

### power.sleep: stop the CPU until an interrupt, in a sleep or deep sleep state
Your calls (2 sites): `__bis_SR_register`, `LPM3_bits`
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

### gpio.release_after_lowpower_reset: release the pins held by the low-power reset state after they are configured
Your calls (1 sites): `PM5CTL0`
- Note: clearing LOCKLPM5 releases the pins after reset and after LPMx.5
- [adapter, reported, from-msp430] After SHUTDOWN wake call SYSCFG_DL_GPIO_init() and then DL_SYSCTL_releaseShutdownIO(); after a normal reset nothing is held, so the PM5CTL0 LOCKLPM5 clear is deleted.
  - Trap: Release the pins only after they are configured, or they glitch to the reset default.
  - Reported only: MSPM0 SDK 2.11.00.07 examples/nortos/LP_MSPM0L1306/driverlib/sysctl_shutdown

### gpio.write: drive an output pin high or low
Your calls (4 sites): `P1OUT`, `P2OUT`, `P3OUT`
- Note: also toggled with ^= for gpio.toggle
- [direct, proven] DL_GPIO_setPins(port, mask) and DL_GPIO_clearPins(port, mask).
  - Trap: The LaunchPad LEDs are active low: clearPins turns the LED on. Check the board's AGENTS.md before porting the polarity.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: LED1 toggles every 98.5 ms
  - Proof (ran): cmsemicon on mspm0l1306: PORT_SetBit and PORT_ClrBit on LED1

## The check, on both boards

Run the same check on the source board and the target and keep both numbers (docs/ports.md, method). Record rows in `data/results/<board>.csv`. A port that builds is not a port that passes.

- gpio.config_pin: read the pin level with and without a load
- clock.system_config: measure a timer or a baud rate against the clock the code assumes; state the clock in the results row
- timer.interval_watchdog_as_timer: one interrupt per second for 30 s, no reset
- clock.low_freq_aclk: an interval timer on LFCLK measured against a wall clock over 30 s
- power.sleep: the wake interrupt runs; measure current for the saving
- wdt.watchdog: unfed watchdog resets, a fed one does not; keep a recovery route before running
- gpio.release_after_lowpower_reset: pin state after wake equals the state set before sleep
- gpio.write: pin level on a meter or logic analyzer
