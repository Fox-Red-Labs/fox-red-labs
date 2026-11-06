# MSPM33 family porting FAQ: errors seen moving code to and from the LP-MSPM33C321A

Errors hit while moving code onto the MSPM33C321A, mostly from MSPM0, with the fix and the run behind each. Board-only errors are in `mspm33c321a/FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### `error: use of undeclared identifier 'MCAN0_INST'` when an MSPM0 app is dropped into the MSPM33 twin project

Carry the app's own `.syscfg` and retarget it instead of keeping the MSPM33 one. The two SDKs name the same instance differently (an MSPM0 LED is `USER_LED_1`, its MSPM33 twin `PASS_LED`); 54 of 62 drop-in failures were names, and carrying the configuration took the build count from 20 to 43. `portability/everywhere/PORTING.md` section 2, matrix X28 and X29.

### The build stops at `DL_I2C_ClockConfig` or on `DL_I2CC_` names

Rewrite the I2C code for UNICOMM. The MSPM33 I2C is the UNICOMM block; its 12 `DL_I2CC_` names are `DL_I2C_CONTROLLER_` and `DL_I2C_` names on the MSPM0, not a prefix swap. `portability/everywhere/PORTING.md` sections 7.1 and 7.2.

### The build stops at `ADC12_0_INST_INT_IRQN`

Write the ADC part against the HSADC. The MSPM33 has no ADC12. The shared three-ways app needed only its `adc_read` rewritten: 37 glue lines, the other four calls unchanged. `portability/three-ways/README.md`.

### SysConfig asks for a DMA instance on every DMA example from MSPM0

Add an MSPM33 DMA instance for each channel in the `.syscfg`. 8 of the 40 configuration ports that did not build stopped here. Matrix X29.

### `Case insensitivity of options has been deprecated; T must be written as t`, then a verify failure at 0x00000020

Change `-T./mspm33c321a.cmd` to `./mspm33c321a.cmd` in the example's makefile. TI Arm Clang 5.1.1 ignores the `-T` form, the image links at 0x20 instead of 0x10000000, and the failed load leaves flash erased. `mspm33c321a/AGENTS.md`.

## Running

### A carried MSPM0 standby example stops with a bus fault, CFSR 0x200, HFSR 0x40

Write the low-power sequence from an MSPM33 example. Suspected: the MSPM0 standby sequence touches registers the MSPM33 lays out differently; not traced. Both hard faults seen on other silicon were this (`sysctl_lfxt_standby`, `timx_timer_mode_periodic_standby`). `portability/everywhere/PORTING.md` section 6.

### The carried app's LED does not light

Assign the RGB LED pins (blue PA2, red PC26, green PC27) in the `.syscfg`. With pin lines commented out, SysConfig picks its own pins. `portability/everywhere/PORTING.md` section 7.1, `mspm33c321a/AGENTS.md`.

### An MSPM33 `gpio_toggle_output` does not build on the LP-MSPM0G3519

Change the 5 app lines that name a port per LED to the single `GPIO_LEDS_PORT`, and assign PB22, PB26 and PB27. The MSPM33's three LEDs sit on three ports, the G3519's on one. `docs/experiments/2026-10-24-mspm33-mspm0-driverlib-both-ways.md`.

### A GPIO set as output always reads 0

Call `DL_GPIO_initDigitalInputFeatures`. The input stage is off until you do. `mspm33c321a/AGENTS.md`.

### A flash write verifies but the old program still runs

Clear the flash caches before the reset, as `mspm33c321a/AGENTS.md` says. A program that turned the caches on leaves old lines in them, and the reset fetches the old vector table. `mspm33c321a/AGENTS.md`.

## Mistakes to avoid

- Do not run TI's `qspi_flash_readback` examples on this kit: they erase part of the flash that holds the demo video.
- Never write the boot configuration at 0x80101800; check the map shows 0 bytes in `BCR_CONFIG` and `BSL_CONFIG`.
