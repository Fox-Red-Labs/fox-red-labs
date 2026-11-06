# Porting pack: original to LP-AM13E230

Contents: what to change (hardest first) and the check.

Terms: [docs/vocab.md](../../../../docs/vocab.md) and the list in the [folder README](../../README.md). 1 KB = 1,024 bytes.

Read this before you port. It lists only what this project needs: the source calls found, the tested replacement for the target, the traps, and the check. Every entry came from a port run on a real board; `proven` means a run or measurement, `built` means it compiled and was not run, `reported` means a vendor document and no run (treat it as a lead, not a result).

Source: family `plain-c`, 226 lines, APIs found: msp430-driverlib, msp430-regs. Target family: AM13. 44 of 44 distinct calls have an entry (100%); 0 do not.

## What to change, hardest first

### uart.rx_byte_blocking: wait for and read one received byte
Your calls (1 sites): `UCA0RXBUF`
- No tested implementation for AM13 (exists for MSPM0).

### clock.fll_dco_trim: tune the internal oscillator to a target frequency with a software trim loop at start
Your calls (11 sites): `DCOFTRIM0`, `DCOFTRIM1`, `DCOFTRIM2`, `DCOFTRIMEN`, `DCORSEL_3`, `FLLD_0`
- [none, reported, from-msp430] No trim loop: the clock tree is set in SysConfig and Device_Init. Instead: the SysConfig SYSCTL block
  - Trap: Do not port Software_Trim().
  - Reported only: SLAAE74B, Migration Guide From MSP430 MCUs to MSPM0 MCUs, Rev. B, June 2025, Table 3-2

### clock.system_config: choose the CPU and bus clock tree at start
Your calls (18 sites): `CSCTL1`, `CSCTL7`, `CSCTL0`, `CSCTL3`, `CSCTL2`, `CSCTL4`
- [redesign, proven] Set the clock tree in the SysConfig SYSCTL block. The default is 32 MHz from SYSOSC; keep it unless a rate needs more.
  - Trap: STM32 code runs at 48 or 64 MHz from a PLL; the ports ran at 32 MHz, so a timing number compares two different CPU clocks. A 56 MHz setting with the bus divider at 1 printed correctly on the UART, but TI's header says that bus clock is to stay under 40 MHz: unknown whether 56 MHz is in spec.
  - Cost: lower CPU clock than the original
  - Proof (measured): stm32-usart-dma-rx-tx on mspm0g3519: left at the 32 MHz default, 6 of 6 host checks at 115200 baud (results `mspm0g3519:uartdma_burst_pps`)
  - Proof (ran): stm32-bootloader on mspm0g3519: left at the SysConfig default 32 MHz (the original runs from an 8 MHz crystal); the 1 s receive timeout and 115200 baud divisors both come from CPUCLK_FREQ; 6 of 6 host checks (`ports/stm32-bootloader/boards/mspm0g3519/Src/uart.c`; results `mspm0g3519:bootloader_checks_passed`)

### uart.init_8n1: open a UART at a baud rate, 8 data bits, no parity, one stop bit
Your calls (10 sites): `UCA0CTLW0`, `UCSWRST`, `UCSSEL__SMCLK`, `UCA0BR0`, `UCA0BR1`, `UCA0MCTLW`
- [redesign, proven] SysConfig UART instance (pins, clock, FIFO, DMA triggers). Baud: with the UART disabled, DL_UART_Main_configBaudRate(inst, UART_0_INST_FREQUENCY, baud), or DL_UART_setBaudRateDivisor(inst, ibrd, fbrd) computed from the clock; then enable.
  - Trap: SysConfig 1.28.0 turns UART internal loopback ON for a new instance on the G5187, so TX echoes into RX; set enableInternalLoopback to false in the .syscfg.
  - Proof (measured): stm32-usart-dma-rx-tx on mspm0g3519: DL_UART_Main_configBaudRate from a single build-time define, 6 of 6 at 115200 (`ports/stm32-usart-dma-rx-tx/boards/mspm0g3519/usart_dma.c`; results `mspm0g3519:uartdma_burst_pps`)
  - Proof (measured): stm32-usart-dma-rx-tx on mspm0g5187: divisor computed from the baud define, 6 of 6 (`ports/stm32-usart-dma-rx-tx/boards/mspm0g5187/main.c`; results `mspm0g5187:uartdma_burst_pps`)

### gpio.config_pin: set a pin as output, input, pull-up or pull-down, or alternate function
Your calls (7 sites): `P1SEL0`, `P1DIR`, `P2DIR`, `P3DIR`, `P1REN`, `P2REN`
- Note: direction, pull enable and function select are three registers on the source and one SysConfig block on the target
- [redesign, proven] A SysConfig GPIO instance per group: direction, pull, interrupt enable and pin assignment. Code only reads, writes and toggles.
  - Trap: Pins are chosen by the migration, not translated: the first UART pins it chose were invalid for the package, and the ADC input moved from PA4 to PA27.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: ports 01 to 05 configure pins in .syscfg

### uart.rx_wake_deep_sleep: a UART receive interrupt wakes the CPU from deep sleep, which echoes the byte
Your calls (5 sites): `UCA0IE`, `UCRXIE`, `__even_in_range`, `UCA0IV`, `UCA0IFG`
- [redesign, reported, from-msp430] UART receive interrupt (SysConfig uart module, registerInterrupt, FIFO on; handler APP_UART_0_INT_Handler) drained in a loop. Wake from STOP or STANDBY on a received byte is not documented in the SDK examples and was not run.
  - Trap: Do not combine with STANDBY on this board until a wake pin is configured: see power.sleep.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/uart/uart_external_loopback_interrupt

### clock.low_freq_aclk: choose the low-frequency clock that keeps running in deep sleep (crystal or internal 32 kHz)
Your calls (18 sites): `CSCTL1`, `CSCTL7`, `CSCTL0`, `CSCTL3`, `CSCTL2`, `CSCTL4`
- [adapter, reported, from-msp430] LFOSC (32 kHz internal) runs the PD0 peripherals in STANDBY; peripherals that run from it (WWDT) need no clock setup.
  - Trap: STOP and STANDBY limit PD0 peripherals to 4 MHz and 32 kHz.
  - Reported only: AM13E2x SDK 26.01.00.03 examples/driverlib/sysctl/sysctl_low_power_modes (README_AM13E230X.md)

### power.sleep: stop the CPU until an interrupt, in a sleep or deep sleep state
Your calls (4 sites): `__bis_SR_register`, `__bic_SR_register`, `LPM3_bits`
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

### uart.tx_byte_blocking: send one byte after the transmit register is free
Your calls (1 sites): `UCA0TXBUF`
- [adapter, proven] DL_UART_Main_transmitData(inst, byte), then while DL_UART_Main_isBusy(inst) wait.
  - Proof (ran): stm32cubeg0-migration on mspm0g3507: each simulated S2 press printed ST's exact message at 115200 baud

### gpio.write: drive an output pin high or low
Your calls (3 sites): `P1OUT`, `P2OUT`, `P3OUT`
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

## The check, on both boards

Run the same check on the source board and the target and keep both numbers (docs/ports.md, method). Record rows in `data/results/<board>.csv`. A port that builds is not a port that passes.

- clock.fll_dco_trim: measured clock
- clock.system_config: measure a timer or a baud rate against the clock the code assumes; state the clock in the results row
- uart.init_8n1: host terminal at that baud shows the text; no echo of what the board sends
- gpio.config_pin: read the pin level with and without a load
- uart.rx_wake_deep_sleep: a typed byte echoes
- clock.low_freq_aclk: interval timer measured against a wall clock
- power.sleep: the wake interrupt runs; the debugger still connects after a reset
- wdt.watchdog: unfed watchdog resets; fed one does not
- timer.interval_watchdog_as_timer: one interrupt per second, no reset
- gpio.release_after_lowpower_reset: pin state after wake equals the state set before sleep
- uart.tx_byte_blocking: host reads the exact bytes
- gpio.write: pin level on a meter or logic analyzer
- time.delay_cycles: pin pulse width at the clock in use
