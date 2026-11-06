# Raspberry Pi Pico SDK examples on the LP-MSPM0G3519 LaunchPad kit

**Works**: five of five Pico SDK examples ran, with their `.c` files exactly as published. `blink_simple` toggles the green LED pin every 250 ms, `hello_world/serial` prints once a second on the probe's serial port, `i2c/bus_scan` finds the touch controller at 0x38 and nothing else, `pwm/led_fade` fades the green LED from its timer interrupt, and `adc/hello_adc` reads A0 twice a second. A 316-line layer of this repo's own, `portability/pico-on-mspm0/`, supplies the Pico SDK headers and calls on TI DriverLib.

Terms: [vocab](../../docs/vocab.md). **Pico SDK** is Raspberry Pi's C SDK for the RP2040 and RP2350; **DriverLib** is TI's C library for the MSPM0 peripherals; **FT6336U** is the touch controller on the Waveshare 3.5 inch panel (I2C address 0x38). 1 kB = 1,024 bytes. Checked 2026-10-25 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS -Os, SysConfig 1.28.0, pico-sdk 2.3.1, on one LP-MSPM0G3519 with the panel fitted, console on the probe's serial port at 115200.

## Results

| Example (`pico-examples/...`) | Status | Flash | RAM | App edits | What ran |
|---|---|---|---|---|---|
| `blink_simple` | Works (pin read, LED not watched) | 2.4 kB | 2.1 kB | 0 | PB27 (Pico GPIO 25) changed 10 times in 12 samples at 4 per second, output enabled |
| `hello_world/serial` | Works | 1.8 kB | 2.1 kB | 0 | `Hello, world!` once a second, 10 lines in 10 s, each ending CR LF |
| `i2c/bus_scan` | Works | 10.6 kB | 2.4 kB | 0 | The full table and `Done.`; 0x38 is the only address that answers, 3 of 3 runs, about 90 ms per scan |
| `pwm/led_fade` | Works (register read, LED not watched) | 3.3 kB | 2.1 kB | 0 | Timer compare read 8 times while running: 52900, 61504, 58564, 55696, 64009, 49729, 34969, 6084, each the square of the fade step the interrupt sets |
| `adc/hello_adc` | Works, path only | 10.4 kB | 2.4 kB | 0 | Banner, then a raw value and volts every 0.5 s. A0 (PA27) is not wired, so the 0.12 to 0.46 V drift is the open pin, not a measurement |

Flash is text plus data from the size tool; RAM is data plus bss, and 2 kB of it is the stack (the Pico SDK default). The chip has 512 kB flash and 128 kB RAM. The rows are in `data/results/mspm0g3519.csv`.

## The layer, per header

| Pico SDK header | Lines (header plus code) | Notes |
|---|---|---|
| `pico/stdlib.h` (with `pico/types.h`) | 32 | start-up before `main()`, `stdio_init_all` |
| `hardware/gpio.h` | 52 | through the board's pin table |
| `hardware/uart.h` | 31 | `printf`, `puts`, `putchar` on UART0 |
| `hardware/i2c.h` | 51 | polled `i2c_read_blocking`, `i2c_init` |
| `hardware/timer.h`, `pico/time.h` | 34 | SysTick microsecond time base, `sleep_ms` |
| `hardware/pwm.h`, `hardware/irq.h` | 71 | timer PWM, wrap interrupt, handler table |
| `hardware/adc.h` | 34 | one conversion per `adc_read` |
| board interface and `boards/mspm0g3519/` | 92 | SysConfig file, pin table, Pico board defaults |

`pico/error.h` and `pico/binary_info.h` come from the real pico-sdk, after the layer on the include path. The pin table, the build, and how to lift the layer into your own project: `portability/pico-on-mspm0/README.md`.

## How bus_scan probes

`bus_scan` reads one byte from each address and counts an acknowledged address as found. The layer sends that read with the controller's ACK bit off, so the controller NACKs the byte and ends the read. A zero-length transfer never comes up, and the Pico SDK's own I2C calls refuse a zero length (`invalid_params_if(HARDWARE_I2C, len == 0)` in `i2c.c`), as this layer's `i2c_read_blocking` does.

## Build and run

```bash
OUT=<build folder> PICO_SDK=<pico-sdk> portability/pico-on-mspm0/build.sh bus_scan
# flash <build folder>/bus_scan.out with DSLite (UniFlash), then read the console at 115200 baud
```

Back up before the first write and restore at the end (`portability/pico-on-mspm0/README.md`, Flash and run).

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| `pwm/led_fade` runs but the compare value stays 0 | A timer reset in the layer's `pwm_init` cleared the wrap interrupt the example enables before it. | Power and reset a slice on its first use only; fixed in `src/pwm.c` |
| Link fails with `symbol "puts" redefined` | The C library keeps `puts` and `fputs` in one object. | Replace `fputs` too |
| A check script prints FAIL while PB27 toggles | It reads the output-enable bit right after its own reset, inside the 0.4 s start-up. | Read the bit over DSLite without a reset |

## Not checked

- No LED was watched. The PWM wrap rate (122 Hz, so one fade up and down in 4.2 s against about 1.1 s on a 125 MHz Pico) is computed, not measured.
- ADC against a known voltage; I2C writes and multi-byte reads; SPI; the other portable pico-examples (I2C sensor and display programs need their parts on the bus); other MSPM0 boards.

## License and source

pico-examples is BSD-3-Clause, Copyright (c) 2020 Raspberry Pi (Trading) Ltd.; the five programs are copied unchanged into `third-party/pico-examples/` with the upstream URL, commit `0d62f75bafc2c8120d3276c3343d1a9195e909e9` (tag sdk-2.3.1) and license in each `SOURCE.md` and `third-party/pico-examples/LICENSE`. The layer is this repo's code and uses the Pico SDK's public function names.
