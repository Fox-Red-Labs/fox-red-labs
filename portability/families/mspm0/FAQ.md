# MSPM0 family porting FAQ: errors seen moving code between LP-MSPM0C1104, LP-MSPM0L1306, LP-MSPM0G3507, LP-MSPM0G3519 and LP-MSPM0G5187

Errors hit while moving code between MSPM0 parts, with the fix and the run behind each. Board-only errors are in each board's own `FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building after a retarget

### `use of undeclared identifier 'UART0'` (or `SPI1`, `TIMA0`, `DMA`) after the device changed in SysConfig

Rename the silicon instance to the alias SysConfig generates (`UART_0_INST`), in the app or in a force-included header. TI's example uses the silicon name, not SysConfig's alias; 41 of the 117 G3519-to-G5187 failures were this (UART 16, SPI 11, I2C 5, TRNG 3, timers 6). `portability/everywhere/PORTING.md` section 2.

### Every port fails with `use of undeclared identifier 'TIMG0_INT_IRQn'` for names the target never generated

Delete TI's pre-generated `ti_msp_dl_config.c` and `.h` that sit beside the source app. The makefile's `-I..` finds them before SysConfig's fresh output. With them deleted, 79 of 122 G3519 examples build for the G5187 (matrix X30). `portability/everywhere/PORTING.md` section 4.

### Most G3507 examples stop when retargeted to the LP-MSPM0L1306

Drop `SYSCTL.forceDefaultClkConfig` from the `.syscfg`. That one G-series property stops 136 of 270 retargets; after it, the stops are peripherals and pins the 32-pin L1306 lacks (PB pins, SPI1, ADC1, the math accelerator, AES, DAC12, RTC). Matrix X15.

### A port builds, but a setting is gone

List the dropped lines before you flash. SysConfig rejects lines the target cannot take, and a port that comments them out builds anyway; 13 of the 96 G3507-to-L1306 builds lost a real setting (an ADC DMA trigger, the crystal clock, a whole DAC12), and 3 L1306-to-G3507 builds lost their ADC reference or channel. Matrix X15 and X17.

## Running a port

### The ported blink toggles a pin, but no LED changes

Assign the target kit's LED pin in the `.syscfg`. A device-only port keeps the source kit's pins: C1104 ports toggled PA22, which is the light-sensor amplifier on the L1306 (matrix X16). The LED pins are PA0 and PA26 on the L1306, PA22 on the C1104, PB22 on the G3507, PB27 on the G3519 and PA17 on the G5187 (each board's `AGENTS.md`).

### An interrupt handler runs on the G3507 but never on the L1306

Use an interrupt line that has a peripheral on the L1306. It implements only lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25 and 31, and pending any other does nothing with no warning. `mspm0l1306/FAQ.md`.

### A UART handler builds on the G5187 and never runs

Name the handler for `UC0`, not `UART0`. The G5187's serial block is UNICOMM and its UART is named `UC0`. `docs/guides/chip-capability-map.md`.

### A retargeted DMA example on the G5187 waits forever, and the destination data is correct

Keep the `DMA_CH0` assignment in the `.syscfg`. SysConfig moved the transfer to channel 1 and the handler tests channel 0. `mspm0g5187/AGENTS.md`, matrix X30.

### `Could not find MEM-AP` after running a ported example on the C1104

The chip is in a boot loop; read the boot diagnostic register (CFG-AP BOOTDIAG), which cycles through values such as 0x07, then run TI's DSSM factory reset after the board's owner agrees. Three ported images have locked the C1104 this way (after `tima_timer_mode_pwm_dead_band`, `smbus_smbus_controller` and `spi_controller_internal_loopback_poll`); the cause of the loop is not found. The chip has no ROM bootloader. `mspm0c1104/FAQ.md`.

### A PWM pin on the G3519 stays low under Zephyr while the timer runs

Define the pin group with SysConfig's pin function (TIMA1.CCP1 on PB27 is function 9). Zephyr's shared MSPM0G1x0x/G3x0x pin file gives function 6, which is COMP2.OUT on the G351x; 136 of 443 peripheral functions in that file differ on this chip. `portability/arduino/zephyr/FAQ.md`.

## Mistakes to avoid

- Count a port as working only after it ran and a check passed; a clean build can still have lost a setting.
- Back up main flash and NONMAIN before the first write, and never write NONMAIN back.
- Run low-power examples last on a part with no ROM bootloader.
