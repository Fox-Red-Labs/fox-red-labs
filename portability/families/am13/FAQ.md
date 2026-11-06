# AM13 family porting FAQ: errors seen moving code to the LP-AM13E230

Errors hit while moving MSPM0, STM32 and C2000 code onto the AM13E230, with the fix and the run behind each. Board-only errors are in `am13e230/FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### `error: use of undeclared identifier 'RX'` in the generated config

Add `#define RX DL_UART_INTERRUPT_RX` to the force-included alias header. The AM13 SysConfig generator accepts a property it does not support and writes the MSPM0 interrupt list's bare word `RX` into C. `portability/everywhere/PORTING.md` sections 3 and 6.

### An MSPM0 example's interrupt or port names are undeclared on the AM13

Use the per-example `board_pins.h` that `portability/everywhere/crossport_mspm0_am13.py` writes, force-included with `-include`. The AM13 generator does not emit the MSPM0 group `_PORT` macro or the `_INT_IRQN` and `_IRQHandler` names; the header maps them (`UART_0_INST` to `UC4_INT_IRQn`, a GPIO group to `GPIO0_INT_IRQn`). Matrix id X31.

### The MSPM0 timer PWM program has no SysConfig module to land on

Copy the MSPM0 generated PWM init into the program and remove the `isTimerWithFourCC` line. AM13 SysConfig has no timer-PWM module and `DL_Timer_PWMConfig` has no `isTimerWithFourCC`. The PWM then read 74.9 and 49.9 percent against 75 and 50. Matrix id X8.

### ST's timer examples fail at the first compile through the STM32G4 HAL layer

Write the timer code against AM13 DriverLib. TI's layer has 65 of the 93 functions its headers declare and no RCC, PWR, timer, ADC or I2C; the two timer examples stop with 90 and 87 errors. Matrix id X9.

## Running

### A usage fault, `CFSR 0x80000`, in code that uses the TMU

Turn the TMU on in `_system_pre_init` (`SCB->CPACR |= (3UL << 0); __DSB(); __ISB(); return 1;`). The compiler moves TMU instructions above `Device_Init()` while the TMU is off; a line at the top of `main()` does not help. `am13e230/AGENTS.md`.

### `HAL_Delay` and STM32 timeouts run much too short or too long

Measure them, or replace them with a DriverLib timer. In TI's STM32G4 HAL layer, `HAL_Delay` and every timeout count CPU cycles, not milliseconds. Matrix id X9.

### The ADC reads 0 on every sample on PA16 with nothing connected

Use PA17 or another input, and check the pin's pull. Suspected: floating or pulled low. The shared app read 0 on PA16 (ADCIN15, the pin TI's ADC example uses) and 157 to 209 on PA17 on the same image. `portability/three-ways/README.md`.

### The board never wakes from STANDBY0 and the debugger cannot connect

Keep a GPIO wake source and a 5 s awake window at boot in any sleep image. A port with only the watchdog interval as wake source never woke, and every connect then failed with DSLite -613, -6310 and -6311; the SDK names a GPIO as the STANDBY wake source. Recovery: `docs/remote-board-recovery.md`. `am13e230/AGENTS.md` trap 9, matrix id X38.

### Motor outputs land on the wrong pins

Read the board revision on the back first: `MCU178E2` is E2; E1 routes MCPWM4 1B, 2A and 3B differently. `am13e230/AGENTS.md`.

## Mistakes to avoid

- Do not halt the core with the debugger while a motor runs: MCPWM4 keeps its last duty and puts a fixed voltage on the windings (`am13e230/foc-drv8376/README.md`).
- DSLite's `-e` means verbose output, not erase.
