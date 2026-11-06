# What goes wrong with Arduino sketches on Zephyr on the LP-EM-CC2340R5 and LP-MSPM0G3519, and how do you fix it?

Every problem met, each as "If you see X, do Y" with the exact text. Page: [README](README.md). Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-24 and 2026-10-25.

**What caused it**, one tag per answer: **Our mistake** (a step in this project was wrong), **Setup or handling** (how the hardware or tools were set up), **Tool or docs** (a missing tool, feature or instruction, or a TI tool such as DSLite acting in a way the docs do not say), **Chip limit** (the silicon behaves this way), **Third-party behavior** (Zephyr, ArduinoCore-zephyr or macOS, not TI), **Unknown**.

## Both boards

### If you see `'isnan' was not declared`, `'atol' was not declared` or `'rand' was not declared`

A Zephyr checkout with no picolibc module falls back to the minimal C library. Add the picolibc module (`build-g3519.sh` does).

### If you see `undeclared identifier 'A0'` or `analogRead` missing

This repo's Arduino layer (`../README.md`) does not have `analogRead`. Use the Zephyr core, or add `analogRead` to the layer.

### If `SPI.transfer(buf, n)` returns zeros or stale bytes in the buffer

ArduinoCore-zephyr `af5b8610f9be` runs `spi_write` and then a separate `spi_read`, so the buffer gets the second transfer's input. Use `SPI.transfer(byte)` in a loop, or change the core to one `spi_transceive`.

### Should I use the Zephyr core or this repo's Arduino layer?

Use the Zephyr core for any chip where Zephyr has the drivers, and this repo's layer for the small MSPM0 parts. RAM and flash decide it. The layer has none of `analogRead`, `analogWrite`, `tone`, `attachInterrupt`, `Serial1` or hardware `Wire`, and each is days of work per chip (`../README.md`). The Zephyr core costs about 3 times the flash of the layer for Blink on the same total basis (22.8 to 27.7 kB against 8.8 kB with the layer's stack and heap reserve; 2.6 kB of code alone) and needs Zephyr, a toolchain and about 80 s a build; the layer builds in about a minute from clean and in seconds after that with TI's compiler. On a part with 4 kB of RAM or 32 kB of flash (MSPM0L1306, C1104) the layer is the only path. The G3507 is not covered by the Zephyr core: Zephyr 4.4.2 has no MSPM0 ADC, I2C or flash driver (ADC is in Zephyr main; the I2C pull request was open). The layer's strength is the Arduino_GFX screen and its FT6336U touch path, which have run on the G3507; the Zephyr core has no equivalent run here. So radio parts (CC2340R5, then the CC13xx/CC26xx boards Zephyr already supports) and the G3519 go through the Zephyr core; the G3507 and smaller MSPM0 parts stay on the layer. The Zephyr core ran `analogRead`, `Serial` and `LED_BUILTIN` from one overlay per board with no new C++ on the CC2340R5 (all drivers upstream in Zephyr 4.4.2) and on the G3519 (with the ADC driver from Zephyr main).

## LP-EM-CC2340R5

### If AnalogReadSerial prints digits with most newlines missing (2.5 kB in 2.5 s containing 6 newlines)

`ZephyrSerial::write()` puts bytes in a 64-byte ring buffer and drops what does not fit. The sketch writes about 5 bytes every 1 ms and 9600 baud carries about 1 byte every 1 ms. Other Arduino cores block here. Apply `serial-blocking-write.patch` (a loop that waits for room): a capture of 1.5 kB holds 185 complete value lines after the boot banner (the last line is cut by the end of the capture), with no merged values; 27.8 kB of flash.

### If the first line of serial output is a few garbage characters

Zephyr prints its boot banner at 115200 before `Serial.begin(9600)`, so a terminal at 9600 shows noise. Ignore the first line.

### If `attachInterrupt` stops firing after a later `pinMode()` on the same pin

Zephyr 4.4.2's `gpio_cc23x0` driver writes the whole IOC register on every pin configure and leaves out the edge-detect bits (IOC10 went from 0x60034000 to 0x60004000). Call `attachInterrupt` again after any `pinMode` on that pin.

### If an I2C scan finds nothing, but a register read at the same address works

The scan sends an empty write to each address; on Zephyr 4.4.2's CC23x0 I2C driver that returns an error even when a device is there (not traced further). MicroPython's `I2C.scan()` shows the same. Probe an address with a one-byte register read instead of an empty write.

### If `analogWrite` on the red LED does nothing when the overlay copies the board file's `pwm-leds` entry (`<&pwm5 0 ...>`)

The red LED pin is muxed to LGPT1 channel 2 (`DIO14_T1_C2N`); the cell after `&pwm5` is the timer channel, so 0 drives channel 0, which reaches no pin. Seen in the MicroPython run: channel 0 wrote LGPT1 C0CC and left C2CC alone. Use `<&pwm5 2 ...>` (this folder's overlay).

## LP-MSPM0G3519

### If the image flashed and verified but nothing blinks, the program counter is at `z_SysNmiOnReset` and `SYSCTL.NMIIIDX` reads 6 (SRAMDED)

Reproduced here on Zephyr main with the default `sram0`: DSLite flashed and started the image, no nRST pulse. The same flash plus only an nRST pulse ran, and so did a build with `zephyr,sram = &sram1`. The start-after-flash path is the cause on this board, not the image or the memory bank. Why that path leaves the SRAM in a bad state is not found. Pulse nRST after the DSLite flash (`mspm0_board_reset` in OpenOCD); `portability/zephyr-mspm0/flash.sh` does it.

### If DSLite says `Connection to MSPM0 core failed` after flashing a Zephyr image

A running Zephyr image hides the core from the debugger. Pulse nRST first, then flash.

### If `attachInterrupt(pin, f, FALLING)` after an earlier `RISING` on the same pin fires on both edges (20 of 10)

Zephyr main's `gpio_mspm0` sets the edge with `DL_GPIO_setLowerPinsPolarity`, which ORs into `POLARITY15_0` and never clears the old bits (they read 11). Clear the pin's two bits before attaching (`POLARITY15_0 &= ~(3 << 2*pin)` for pins 0 to 15): 10 of 10. The driver fix is to clear them first.

### If `analogWrite` gives the opposite brightness (value 192 measures 25 percent high)

Zephyr main's `pwm_mspm0` writes the pulse into the compare register in edge-aligned down count, where load sets the pin and compare clears it, so the pulse becomes the low time. Write `255 - value`, or fix the driver to write period minus pulse.

### If `analogWrite` on PB27 leaves the pin low although the timer counts

Zephyr's shared MSPM0G1x0x/G3x0x pin file gives `tima1_ccp1_pb27` function 6, which is COMP2.OUT on the G351x. Define the pin group with `MSP_PINMUX(58, MSPM0_PIN_FUNCTION_9)` (this folder's G3519 overlay); check any pin group against SysConfig's `MSPM0G351X.json`. A script check of the shared pin file against SysConfig 1.28.0 found 136 of 443 peripheral functions that differ on this chip; the pins this folder uses (UART0, SPI0, I2C2) match.

### If setting the SPI loopback bit (`LBM` in `CTL1`) on a running SPI block has no effect

The SPI loopback bit only takes while the block is disabled. Clear `ENABLE`, set `LBM`, set `ENABLE` again.
