# dataset-stream board files: what each LaunchPad needs for the BMI160 on BP-BASSENSORSMKII

Terms: [docs/vocab.md](../../../docs/vocab.md). A BoosterPack plug-in module is a board that plugs on top of a LaunchPad development kit. I2C is the two-wire bus the sensor talks on: SCL is its clock wire, SDA its data wire, and the LaunchPad is the controller that starts every transfer. A pull-up is a resistor that holds a wire high when nothing drives it low. J numbers (J8, J21, J101) are jumper blocks printed on the LaunchPad. SysConfig is TI's tool that writes a chip's pin setup. A boot-mode pin is one the chip reads at reset to decide where to start. 1 kB = 1,024 bytes. Cause tags: **Tool or docs** (a tool or page is missing or wrong), **TI example default** (a TI example's setting causes it), **Setup or handling** (how the board is set up), **Chip limit** (the silicon cannot do it), **Unknown** (not found; "suspected" names the lead).

Each board supplies a microsecond counter, a serial write, two I2C functions and a delay; the table gives the pins. Nothing here is built or verified (2026-10-08): the BoosterPack plug-in module has not arrived, so every pin below is read from documents, not run. Sources: TI user guide SLAU829 (the BoosterPack's header pins), the board files in TI's SDKs, the LaunchPad user guides, and for the LP-MSPM0G3507 TI's `data_sensor_aggregator` SysConfig file in MSPM0 SDK 2.11.00.07.

Every board supplies four things to `core/`:

1. `board_micros()`: a counter that goes up by one every microsecond and wraps at 2^32. Use a 32-bit timer clocked at 1 MHz. (The Cortex-M0+ SysTick counter is only 24 bits and wraps in about half a second at 32 MHz, so on its own it is not enough.)
2. `uart_write(buf, n)`: the serial port the debug probe passes to the Mac, 115200 8N1. On the Mac it is `/dev/cu.usbmodem<probe serial>1`.
3. `i2c_write` and `i2c_read` for `bmi160_bus`: controller mode, 7-bit address 0x69, 100 or 400 kHz. The BoosterPack has 10 kOhm pull-ups on SCL and SDA.
4. `delay_ms` for the BMI160's wake-up (4 ms and 81 ms, once).

The BMI160 is powered from header pin 1 (3.3 V) with no switch, so no power pin is needed for it.

| Host | I2C (header pin 9 SCL, pin 10 SDA) | Serial to the Mac | Changes needed | TI code to start from | Verified |
|---|---|---|---|---|---|
| LP-MSPM0G3507 | I2C1, PB2 SCL, PB3 SDA | UART0, PA10 TX, PA11 RX (J21, J22 up) | Remove (pull off) jumpers J8, J9, J16, J17, J18: they connect LaunchPad parts to BoosterPack pins 26, 6, 24, 8, 5 | `examples/nortos/LP_MSPM0G3507/msp_subsystems/data_sensor_aggregator` for the I2C setup | No |
| LP-MSPM0G5187 | I2C1, PA9 SCL, PA8 SDA | UART0, PA10 TX, PA11 RX through J101 | None known | `edgeAI/timeseries_data_capture` for UART; I2C from SysConfig | No |
| LP-MSPM0L1306 | I2C0, PA1 SCL, PA0 SDA | UART0 through the debug probe | Remove jumper J2 (its red LED is on PA0, the data line), and J4 to J6, J1, J11, J3 (parts on BoosterPack pins 2, 5, 8, 23, 26, 39) | none found in the SDK; not run | No |
| LP-EM-CC2340R5 | DIO24 SCL, DIO0 SDA | Backchannel UART through the debug probe | None (TI's board notes: "No modifications are needed"); not run | F3 SDK `drivers/i2ccontroller` | No |
| LP-CC1354P10-1 | DIO4 SCL, DIO5 SDA | Backchannel UART through the debug probe | None | F2 SDK `rtos/drivers/i2ctmp` for the I2C setup | No |
| LAUNCHXL-F2800137 | Not on pins 9 and 10; two wires (see below) | Backchannel serial port through the debug probe | Two wires | none | No |

LAUNCHXL-F2800137 wiring (not run): its site 1 has no I2C on pins 9 and 10, so run two wires on the BoosterPack's top pins, pin 9 to header pin 18 (GPIO32) and pin 10 to header pin 34 (GPIO33). C2000Ware 26.02 `pin_map.h` lists I2CA on GPIO32 and GPIO33. GPIO32 is a boot-mode pin; the BoosterPack's 10 kOhm pull-up holds it high, the flash-boot level.

Notes for whoever writes the first board file:

- TI's `data_sensor_aggregator` reads the BMI160's data registers but never writes its command register (no write to 0x7E in `data_sensor_aggregator.c`). The data sheet says the accelerometer and gyroscope start in suspend mode, where no sampling takes place. **Unknown (suspected: TI example default).** Its motion values may read as zero until the two wake-up commands in `core/bmi160.c` are sent. Not run.
- The same example waits `delay_cycles(5000000)` around each I2C transfer: 156 ms each at 32 MHz. Do not copy those waits into a 100 Hz loop; poll the I2C status instead.
- The trained model's scratch RAM (4.4 kB for the 3-channel, 128-sample `tiny` model, from `tools/dataset/train.py --gen-c`) does not fit next to the stack in the LP-MSPM0L1306's 4 kB of RAM. Stream from the L1306; run the model on a G3507 or G5187.
- The BOOSTXL-EDUMKII's accelerometer is analog (Kionix KXTC9-2050 on header pins 23, 24, 25): its board file reads three ADC channels instead of I2C, and the rest of `core/ds_stream` is the same. Its scale is not 16,384 counts per g; read it from the KXTC9-2050 data sheet and pass it to `train.py --scale`.

## Questions

### Which jumpers do I pull on the LP-MSPM0G3507 for the BMI160 BoosterPack?

J8, J9, J16, J17 and J18. They connect the LaunchPad kit's button, thermistor and light sensor to BoosterPack pins 26, 6, 24, 8 and 5, which the BoosterPack also drives. From the documents; not run.

### Which jumpers do I pull on the LP-MSPM0L1306?

J2 first: its red LED sits on PA0, the I2C data line, and pulls it down. Then J4 to J6, J1, J11 and J3, which connect LaunchPad parts to BoosterPack pins 2, 5, 8, 23, 26 and 39. From the LaunchPad user guide; not run.

### Do I change anything on the LP-MSPM0G5187?

None known from the documents: its I2C1 pins (PA9, PA8) and UART0 through J101 need no jumper changes for this BoosterPack. Not checked on a board; the G5187 has not arrived.

### Which serial port carries the samples on the LP-MSPM0G3507?

UART0 on PA10 (TX) and PA11 (RX), passed to the Mac by the debug probe when jumpers J21 and J22 are in their up position. On the Mac it is the probe's port whose name ends in 1, such as `/dev/cu.usbmodemMG3500011`. From the LaunchPad user guide; not run with this code.

### Why not use SysTick for board_micros()?

Use a 32-bit timer at 1 MHz instead. The Cortex-M0+ SysTick counter is only 24 bits, so at 32 MHz it wraps in about half a second, too soon for `ds_due` to tell the time without help.

### Why is TI's data_sensor_aggregator so slow?

It waits `delay_cycles(5000000)` around each I2C transfer, 156 ms each at 32 MHz. Poll the I2C status instead when you want 100 samples a second.

### Can I stream from the LAUNCHXL-F2800137?

Only with two wires to GPIO32 and GPIO33 (above). GPIO32 is a boot-mode pin, held at the flash-boot level by the BoosterPack's pull-up.

### Can the LP-MSPM0L1306 run the trained model as well as stream?

Not the default model: see "Will the model run on the LP-MSPM0L1306?" in [the guide](../../../docs/guides/collect-a-dataset.md#questions). It needs 4.4 kB of RAM and the chip has 4 kB.
