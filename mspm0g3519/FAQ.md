# TI LP-MSPM0G3519 LaunchPad on a Mac: errors and fixes

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes. Measured on the board, 2026-10-08.

## Can the MSPM0G3519 be flashed with OpenOCD from a Mac?

Yes. The xPack OpenOCD 0.12.0-7 build used elsewhere in this repo has no MSPM0G3519 in its part table, and prints `Unidentified PART[0x1508]/variant[0x16], unknown DeviceID[0xbba9]. Attempting to proceed as mspm0x.`, but then programs the board: a 600-byte blink took 0.6 s to program and verify (the first run after a UniFlash load took 20.6 s because OpenOCD's CRC check timed out before it fell back to a byte compare). Zephyr's `west flash` for this board uses the same OpenOCD and worked. If you see the "Unidentified PART" line, ignore it.

## DSLite says a firmware update is required for the XDS110 probe. What now?

A new board can ship with probe firmware 3.0.0.31, and UniFlash 9.6's command line stops with `A firmware update is required for the XDS110 probe` and error -264. Run `xdsdfu -e` to see the firmware. If it is below 3.0.0.43, from `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/` run `./xdsdfu -I <serial> -m`, then `./xdsdfu -I 00000000 -f firmware_3.0.0.43.bin -s <serial> -r`. The probe's serial number reads 00000000 only while the update runs. The update changes the probe, not the chip.

## Zephyr says this board has 256 kB of flash and 64 kB of RAM. Is that right?

The chip has 512 kB of flash and 128 kB of RAM in two banks. Zephyr 4.4.2's `lp_mspm0g3519` board describes the first bank only, so its build summary reads `FLASH: 256 KB` and `RAM: 64 KB`. A Zephyr application has half the memory the chip has.

## The Zephyr blinky builds and flashes, but the LED does not blink. What is wrong?

`west build -b lp_mspm0g3519 samples/basic/blinky` and `west flash` both finish without an error, and nothing blinks. Read the chip with OpenOCD and the program counter sits at `z_SysNmiOnReset`, the active exception is 2 (NMI) and `SYSCTL.NMIIIDX` reads 6, which TI's header names SRAMDED (SRAM double-error detect). A later run (2026-10-25) found the trigger: the NMI follows DSLite's start-after-flash path, and an nRST pulse after the flash (`mspm0_board_reset` in OpenOCD) starts the same image normally (`portability/arduino/zephyr/FAQ.md`). Why that path leaves the SRAM in error is not found (checked 2026-10-08, Zephyr 4.4.2, three builds). The larger benchmark app built for the same board did run. To get a first blink, use TI's SDK example (`gpio_toggle_output`) as in the [README](README.md).

## OpenOCD says `Unknown Device ID[0x0], cannot identify target` and `auto_probe failed`. What now?

OpenOCD could not read the chip's identity, so it refused to program. The same read returns zeros with UniFlash's `DSLite memory` too, so it comes from the chip's state after some programs have run, not from OpenOCD. Load any program with UniFlash (`dslite.sh --mode flash ...`), and OpenOCD can read and program again. It appeared after the timing image ran and intermittently between CoreMark runs.

## UniFlash stops with `Error -260 ... An attempt to connect to the XDS110 failed` although `xdsdfu -e` lists the board. What now?

Another program was using UniFlash's debug server at that moment (here, other boards being flashed in parallel). Run the command again; a retry loop of up to six tries, a few seconds apart, always got through.

## TI's internal temperature example prints about 507 instead of room temperature. Is the chip broken?

No. On this board `adc12_internal_temp_sensor_mathacl` reads raw ADC codes of about 815 (the LP-MSPM0G3507 reads 811) but its own conversion gives 507, because the factory trim constant it uses is 1881 here against 795 there. The cause is unknown. Treat the raw code as a relative reading until TI explains it (checked 2026-10-08, SDK 2.11.00.07).

## The UART loopback fails at some speeds near 5 to 6 Mbaud. Is that normal?

On this board, with UART0 PA10 wired to PA11 and 2 kB of random bytes per speed, only 5.5 Mbaud (1,453 correct) and 6.0 Mbaud (1,921 correct) failed; 9,600 baud to 5.33 Mbaud and 6.4 to 10.67 Mbaud were clean, and 12 Mbaud sends nothing. The LP-MSPM0G3507 failed at different speeds (5.0 and 5.33 Mbaud as well) in the same test. Repeated twice with identical counts. If you need a speed in that range, test it on your wiring and choose a neighboring speed that passes.

## OpenOCD reads of RAM keep restarting my program on the MSPM0G3519. How do I read results from a running image?

Every OpenOCD connect through the on-board XDS110 resets this board, so a poll loop that opens OpenOCD each few seconds restarts the image each time and the counters never advance. DSLite's `memory` command (`DSLite memory -c <ccxml> -r 0x20200000,64 -s 8 -o out.bin`) reads RAM without a reset, so the program keeps running. It worked for a 95-second MP3 decode run polled every few seconds (2026-10-21). A DSLite flash starts the image by itself.

## Can the MSPM0G3519 decode an MP3 in real time?

No, not with minimp3 in float. At 80 MHz it takes 9.84 times the audio length for a 128 kbps stereo clip and 7.38 for a 64 kbps mono clip, with every frame bit-exact against a Mac build; it would need about 787 MHz to keep up (`ports/minimp3/README.md`). A fixed-point decoder is the way to try it.

### A TI MSPM33 `gpio_toggle_output` carried to the G3519 stops with `GPIO_LEDS_USER_LED_2_PORT` undeclared. What now?

The MSPM33 example puts its three LEDs on three ports and names a port per LED. The G3519 LEDs are all on port B, so SysConfig writes a single `GPIO_LEDS_PORT`. Change the five lines that use `GPIO_LEDS_USER_LED_n_PORT` to `GPIO_LEDS_PORT` and assign the pins to PB22, PB26 and PB27 in the `.syscfg`. With that, all three pins toggle (checked over the probe 2026-10-24).

### A carried-over MSPM33 UART example waits forever on the G3519. What is wrong?

It expects a wire from TX to RX. Add `UART1.enableInternalLoopback = true;` to the `.syscfg` and keep the pins off PA10 to PA13 (the console and CAN header pins) by reserving them as inputs in the configuration (`portability/everywhere/crossport_pair.py --reserve`). Then `rxPacket` reads `MSP!`. MSPM33 I2C (`DL_I2CC_` names) and HSADC examples do not build on the G3519 (`portability/everywhere/PORTING.md` section 7.2). Checked 2026-10-24.

### TI's UART console example prints nothing on the G3519. What do I check?

 The example sends on PA10 and reads on PA11. Those two pins reach the probe's USB serial port only through jumpers J12 and J13 on the XDS side, and a wire on J4 PA10 to PA13 shares them. Take the wires off those pins, leave J12 and J13 on XDS, flash, and read the probe's first port (`/dev/cu.usbmodem<serial>1`) at 115200 8N1 with a capture that holds the port open. You should see `==== MSPM0 Console Test ====` and then `> Testing...0`, `1`, `2` about once a second. On the SDK's SysConfig 1.28.0 the build works but this repo's wrapper refuses it until `FRL_SYSCONFIG` points at it. Checked 2026-10-24.

### A Zephyr PWM, timer or other peripheral pin on the G3519 stays low although the peripheral runs. What is wrong?

 Zephyr's `lp_mspm0g3519` board includes the pin file shared with the MSPM0G1x0x and G3x0x chips, and many of its peripheral function numbers are different on the G351x. On PB27, `tima1_ccp1_pb27` selects function 6, which is COMP2.OUT on this chip, so the timer counts and the pin never moves; TIMA1.CCP1 is function 9. If you see this, define the pin group in your overlay from SysConfig's device data (`MSPM0G351X.json`), for example `pinmux = <MSP_PINMUX(58, MSPM0_PIN_FUNCTION_9)>;`. A script check found 136 of 443 entries that differ, including the board file's CAN-FD pins PA12 and PA13; UART0, SPI0, SPI1, I2C1 and the BoosterPack I2C2 pins (PC2, PC3) match (checked 2026-10-25).

### On Zephyr, `FALLING` after `RISING` on the same G3519 pin fires on both edges, and `analogWrite` is inverted. Why?

 Zephyr main's MSPM0 GPIO driver adds the new edge bits to the old ones and never clears them, and its PWM driver writes the pulse as the low time. If you see double interrupts, clear the pin's two bits in `POLARITY15_0` (pins 0 to 15) or `POLARITY31_16` before attaching. If you see inverted brightness, write `255 - value` until the driver is fixed. Checked 2026-10-25 on the board (`portability/arduino/zephyr/README.md`).

### DSLite says "Connection to MSPM0 core failed" after a Zephyr image was flashed to the G3519. What now?

 A running Zephyr image leaves the core hidden from the debugger (OpenOCD says "Could not find MEM-AP"). If you see this, pulse nRST first (`openocd ... -c init -c mspm0_board_reset -c shutdown`), then flash; `portability/zephyr-mspm0/flash.sh <image.hex>` does the pulse, a flash from a `.bin` padded to a multiple of 8 bytes, and a second pulse. DSLite also refuses a block whose length is not a multiple of 8 ("Length of block is 14860, but it should be divisible by 8"). `portability/zephyr-mspm0/README.md`, checked 2026-10-24.

### On Arduino-on-Zephyr, `SPI.transfer(buf, n)` returns zeros on the G3519, and the SPI loopback bit does nothing. Why?

 ArduinoCore-zephyr `af5b8610f9be` runs `spi_write` and then a separate `spi_read`, so the buffer receives the second transfer's input: `transfer(buf, 16)` returned 0 of 16 correct, while 256 of 256 single-byte `transfer(byte)` calls and `transfer16(0xA55A)` came back equal. If you see this, call `SPI.transfer(byte)` in a loop. For a test without a wire, set the SPI block's loopback bit (`CTL1.LBM`) while the block is disabled: clear `ENABLE`, set `LBM`, set `ENABLE` again; set on a running block it has no effect. Checked 2026-10-25 (`portability/arduino/zephyr/README.md`).

### The portable ADC app on the G3519 reads 900 to 2,600 counts on PA25 with nothing connected. Which input is steady?

 On the G5187 the same floating PA25 reads a steady 1,450. On this board it swings in a repeating pattern, so something on the header or the stacked BoosterPack drives it. The app's ADC channel 11 is the internal temperature sensor and needs no wire: set `ADC121.adcMem0chansel = "DL_ADC12_INPUT_CHAN_11"` and `ADC121.disChan11 = true` in the `.syscfg`, with the 2.5 V internal reference. It reads 1,085 to 1,086 counts, steady. Checked 2026-10-25.

### TI's `adc_singlechannel` FreeRTOS example for the G3519 prints only `Starting the adcsinglechannel example`. Is my build wrong?

 No. The SDK 2.11.00.07 example built unchanged for `LP_MSPM0G3519` and the F3 SDK's `adcsinglechannel` source built in the same project both print that one line and never a result; `adcValue0` stays 0 and the threads end (program counter in `vPortFree`, link register in `pthread_exit`). Cause not found. If you need an ADC reading under FreeRTOS on this board, use DriverLib calls as the three-ways app does. Checked 2026-10-25.

### An F3 TI Drivers example built in the G3519 project runs but its LED never lights. Why?

 The F3 sources call `GPIO_write(CONFIG_GPIO_LED_0, ...)` and rely on the board table to make PA0 an output. In the MSPM0 table for `i2c_controller` and `i2c_target`, PA0 is `GPIO_DO_NOT_CONFIG`, so the output enable stays 0. If you see this, call `GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW)` before the first write, as TI's `gpio_interrupt` for this board does. The ten twins: `empty`, `uart2echo` and `uart2callback` run and show their behavior; six wait for hardware (a button, an I2C or SPI partner, a sensor); `adcsinglechannel` is silent (above). Checked 2026-10-25.

### A Renesas FSP example (I2C master) runs on the G3519 but the first transfer to the Waveshare touch controller fails or the read never ends. What is wrong?

 Two causes, both seen on 2026-10-25. The FT6336U holds the I2C bus busy for about 65 ms after its reset pin (PB11) goes high, so a transfer started right after boot completes late; hold PB11 low 5 ms, release it and wait 400 ms before the first transfer. And a controller read with the ACK bit set acknowledges the last byte, the slave keeps sending and the bus stays busy; leave the ACK bit off on reads. If you see the example print `Error in reading Device ID` followed by 65535 in every axis, the device on the bus is not the one the example was written for (`ports/renesas-ra-fsp/README.md`).

### An ADC on a timer event converts once and stops, or its DMA transfers never start, or they stop after the first buffer. What is wrong?

 Three settings, seen on 2026-10-25. In sequence mode the ADC clears its enable bit after one sequence; use repeat mode and set the last conversion in the sequence to wait for the next trigger (`ADC121.repeatMode = true; ADC121.adcMem7trig = "DL_ADC12_TRIGGER_MODE_TRIGGER_NEXT";` in SysConfig). A DMA channel on the ADC trigger sees nothing until the DMA sample count is 1 (`DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1)`). The ADC clears its DMA enable bit when a transfer finishes, so call `DL_ADC12_enableDMA` again each time you arm the channel. If you put two or more DMA channels on the one ADC trigger, expect them to miss triggers: here a four-channel setup captured half the samples on the first group and none on the second, cause not found; one channel on the trigger captured 99.9 percent of the samples (`ports/renesas-ra-fsp/README.md`).

### A timer period measures about half a percent short on the host clock. Is the timer wrong?

 On this board 100 ms, 1 s, 1 ms and 2 ms periods from the 32 MHz oscillator measured 0.4 to 0.65 percent short against the Mac's clock, the same offset at every setting, so the counts are exact and the oscillator runs about 0.55 percent fast. If you see this, check your own board against a reference before relying on the period to better than 1 percent (2026-10-25).

### A Raspberry Pi Pico SDK example built for the G3519 stops at link with `symbol "puts" redefined`, or its stack is 256 B. What now?

 TI Arm Clang's C library keeps `puts` and `fputs` in one object (`fputs.c.obj`), so a layer that replaces only `printf`, `puts` and `putchar` pulls that object in and the link fails; replace `fputs` too. The SysConfig `device_linker.cmd` sets `--stack_size=256`, and a `-Wl,--stack_size` placed before it on the link line is overridden; put `-Wl,--stack_size=0x800` after it to get the Pico SDK's 2 kB default. With both, `blink_simple`, `hello_world/serial`, `i2c/bus_scan`, `pwm/led_fade` and `adc/hello_adc` build and run with their `.c` files unchanged, and `bus_scan` finds only the touch controller at 0x38 (`ports/pico-sdk/README.md`, checked 2026-10-25).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A running program leaves the factory region unreadable (all zeros) in a low-power state (`tools/frl/target.py`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| OpenOCD reset halt, read, reset run (writes nothing) (the program runs again afterwards) | remote | proven | `tools/frl/target.py`, 2026-10-24 |

Restore: DSLite: dslite.sh --mode flash -e -f -v backup-flash.bin,0x0, then a power cycle, read back and compare. Regions: main flash. (`mspm0g3519/AGENTS.md`)

<!-- recovery:end -->
