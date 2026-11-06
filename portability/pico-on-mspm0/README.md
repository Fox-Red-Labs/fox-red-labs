# Pico SDK calls on MSPM0 DriverLib

Terms: [vocab](../../docs/vocab.md). **Pico SDK** is Raspberry Pi's C SDK for the RP2040 and RP2350; **DriverLib** is TI's C library for the MSPM0 peripherals; **SysConfig** is TI's tool that writes a chip's pin and clock setup code.

Five programs from `raspberrypi/pico-examples` build and run on the LP-MSPM0G3519 with their `.c` files unchanged: `blink_simple`, `hello_world/serial`, `i2c/bus_scan`, `pwm/led_fade` and `adc/hello_adc`. This folder supplies the Pico SDK headers those programs include and the calls they make, on DriverLib, bare metal. It is driver code (I2C transfers, a PWM timer with its interrupt, a microsecond time base), not a rename shim, so all of it counts as written lines. Results: `ports/pico-sdk/README.md`.

## What is here

| File | Lines | What it does |
|---|---|---|
| `include/pico/stdlib.h`, `include/pico/types.h`, `src/runtime.c` | 32 | `stdio_init_all`, `tight_loop_contents`, `uint`; SysConfig, SysTick and the board's start-up run as a C constructor, so the example's `main()` stays the program's `main()` |
| `include/hardware/gpio.h`, `src/gpio.c` | 52 | `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_set_function` (I2C, UART, PWM, SIO), `gpio_pull_up`, through the board's pin table |
| `include/hardware/uart.h`, `src/stdio_uart.c` | 31 | `uart0`, `uart_putc_raw`; `printf`, `puts`, `fputs` and `putchar` on UART0, `\n` sent as `\r\n` as the Pico SDK does |
| `include/hardware/i2c.h`, `src/i2c.c` | 51 | `i2c0`, `i2c_init` (sets the SCL period), `i2c_read_blocking`, polled, with a 10 ms limit per transfer |
| `include/hardware/timer.h`, `include/pico/time.h`, `src/time.c` | 34 | `time_us_64`, `busy_wait_us`, `sleep_us`, `sleep_ms` on SysTick at 1 kHz plus its down-counter |
| `include/hardware/pwm.h`, `include/hardware/irq.h`, `src/pwm.c` | 71 | PWM slices as timers, `pwm_init`, `pwm_set_gpio_level` (takes effect at the wrap, as on the Pico), wrap interrupt, `irq_set_exclusive_handler`, `irq_set_enabled` |
| `include/hardware/adc.h`, `src/adc.c` | 34 | `adc_init`, `adc_gpio_init`, `adc_select_input`, `adc_read` on ADC0 |
| `src/pico_mspm0_board.h` | 11 | what the layer needs from a board folder |
| `boards/mspm0g3519/` | 81 | `board.syscfg` (46), `pico_board.h` (14, the Pico board defaults), `pins.c` (21, the pin table and the touch-controller reset) |
| `build.sh` | 37 | builds one example |

Layer total 316 lines, board 81. `pico/error.h` and `pico/binary_info.h` are the real Pico SDK headers: this folder's `include/` comes first on the include path, and anything it does not provide falls through to the SDK. No `-include` flag is used. Only the calls the five examples make are here.

## The pin table

Pico programs use GPIO numbers 0 to 29. `boards/mspm0g3519/pins.c` maps the ones these examples touch; any other number is ignored.

| Pico GPIO | Pico SDK role | LP-MSPM0G3519 pin | Peripheral |
|---|---|---|---|
| 0 | `PICO_DEFAULT_UART_TX_PIN` | PA10 | UART0 TX, the probe's serial port |
| 1 | `PICO_DEFAULT_UART_RX_PIN` | PA11 | UART0 RX |
| 4 | `PICO_DEFAULT_I2C_SDA_PIN` | PC3 | I2C2 SDA (`i2c0`, `i2c_default`) |
| 5 | `PICO_DEFAULT_I2C_SCL_PIN` | PC2 | I2C2 SCL |
| 25 | `PICO_DEFAULT_LED_PIN` | PB27 | green LED; PWM slice 0 channel B = TIMA1 compare 1 |
| 26 | ADC input 0 | PA27 | ADC0 channel 0 |

ADC input 4 (the Pico's temperature sensor) is not mapped. PB11 is the Waveshare panel's touch reset: the board start-up holds it low 5 ms, then high, and waits 400 ms before `main()`.

## Set up once

`build.sh` reads these variables, falls back to the default shown, and stops with one line naming the variable when a path is missing.

| Variable | What it is | How to get it | Default folder | Size |
|---|---|---|---|---|
| `SDK` | MSPM0 SDK 2.11.00.07 (DriverLib, CMSIS headers, startup file) | TI's download page for the MSPM0 SDK | `/Applications/ti/mspm0_sdk_2_11_00_07` | 0.9 GB |
| `CGT` | TI Arm Clang 5.1.1.LTS (comes with CCStudio 21) | install CCStudio, or TI's compiler download | `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS` | 0.4 GB |
| `SYSCFG` | `sysconfig_cli.sh`, SysConfig 1.28.0 (1.26.3 is used if 1.28.0 is missing) | TI's SysConfig download, or the copy inside CCStudio | `/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh` | 0.2 GB |
| `PICO_SDK` | a pico-sdk checkout, 2.3.1 (two headers are used) | `git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git` | any folder (the script's default is an external drive, so set it) | 40 MB |
| `OUT` | build folder, about 1 MB per example | created by the build; any folder whose parent exists | any folder (set it) | 1 MB |

## Build

```bash
OUT=<build folder> PICO_SDK=<pico-sdk> portability/pico-on-mspm0/build.sh bus_scan     # or blink_simple, hello_serial, led_fade, hello_adc
```

A name builds `third-party/pico-examples/<name>/`; a folder path builds the `.c` files in that folder. The output is `$OUT/<name>.out` with `build.log` beside it; the script prints the size. A build takes under 30 seconds.

## Flash and run

Back up the board's flash before the first write; the Back up and Restore rows in `mspm0g3519/AGENTS.md` give the DSLite steps. Then flash the `.out` file with UniFlash DSLite, using `mspm0g3519/MSPM0G3519_<probe serial>.ccxml` with your probe serial:

```bash
<UniFlash>/dslite.sh --mode flash -c MSPM0G3519_<probe serial>.ccxml -e -f -v <out>/bus_scan.out
```

Press the board's nRST button after the flash. Open the probe's serial port (`/dev/cu.usbmodem<probe serial>1`, 115200) in a terminal program before you press it, so the first line is not missed. Restore the backup when you are done.

## Take it with you

Copy these into your project, keeping the folder layout:

- `build.sh`
- `include/` (all ten headers)
- `src/` (seven `.c` files and `pico_mspm0_board.h`)
- `boards/mspm0g3519/board.syscfg`, `pico_board.h`, `pins.c`

What they need: the MSPM0 SDK, TI Arm Clang and SysConfig from "Set up once", and a pico-sdk checkout for `pico/error.h` and `pico/binary_info.h`. The include order is what makes it work: `include/`, then `boards/<board>/`, then the SDK's `src/common/pico_base_headers/include` and `src/common/pico_binary_info/include`, then the layer's `src/`, the generated SysConfig folder and the MSPM0 SDK. No defines and no force-include flag are needed. Link with `-Wl,--stack_size=0x800` after the generated `device_linker.cmd` (the Pico default stack is 2 kB; the generated file asks for 256 B).

Then, in a fresh folder with the files above in `pico-on-mspm0/`:

```bash
git clone --depth 1 https://github.com/raspberrypi/pico-examples.git
export SDK=<MSPM0 SDK> CGT=<TI Arm Clang> SYSCFG=<sysconfig_cli.sh> PICO_SDK=<pico-sdk>
OUT=$PWD/build pico-on-mspm0/build.sh pico-examples/blink_simple
```

This was run in a folder outside this repository with only the files listed: `blink_simple.out` came out at the same size as the in-repo build (2.4 kB of code).

For another MSPM0 board, copy `boards/mspm0g3519/` to `boards/<board>/` and change:

- `board.syscfg`: the device lines at the top, and the UART, I2C, ADC and GPIO pin assignments.
- `pins.c`: each row of `pico_pins` (port, pin bit, `IOMUX_PINCMn` index and the `IOMUX_PINCMn_PF_*` function names from the device header), `pico_pwm_slices` (the timer behind slice 0), `pico_adc_inputs`, and `pico_board_init` (remove the touch reset if the board has no panel).
- `pico_board.h`: `PICO_MSPM0_PWM_IRQN` to that timer's IRQ number, and the defaults if your GPIO numbers differ.
- `build.sh`: the `BOARD=` line and the startup file name (`startup_mspm0g351x_ticlang.c`).

The interrupt entry in `src/pwm.c` is `TIMA1_IRQHandler`; rename it if your slice 0 timer is another.

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| Link fails: `symbol "puts" redefined` in `libc.a<fputs.c.obj>` | The C library keeps `puts` and `fputs` in one object, so replacing only `puts` pulls in both. | Replace `fputs` too (`src/stdio_uart.c`) |
| `pwm/led_fade` runs but the compare value stays 0 | The example enables the wrap interrupt before `pwm_init`, and a timer reset inside `pwm_init` cleared the interrupt mask. | Power and reset a slice on its first use, never inside `pwm_init` (`src/pwm.c`) |
| `tools/check/blink.py` prints FAIL while PB27 toggles | The script reads the output-enable bit right after its own reset, before the 0.4 s start-up ends. | Read the bit over DSLite without a reset, or count the toggles |
| First line of a capture is cut off or starts with a stray byte | It is the end of the run before the capture's reset. | Read from the first full line after the reset |

## Not checked

- No LED was watched; the LED and PWM results are pin and register reads.
- The PWM wrap rate was computed (122 Hz at 32 MHz with divider 4), not measured. The fractional part of the PWM clock divider is dropped.
- ADC against a known voltage; ADC inputs 1 to 4.
- I2C writes, reads longer than one byte, zero-length transfers (`bus_scan` probes with a 1-byte read, so it never asks for one), and speeds other than 100 kHz.
- SPI, UART reads, `stdio` input, the Pico's other 61 portable examples, and any board but the LP-MSPM0G3519.

## License and source

The layer is this repo's code and carries no Pico SDK source; it uses the Pico SDK's public function names. The pico-examples programs it serves are BSD-3-Clause, Copyright (c) 2020 Raspberry Pi (Trading) Ltd. (`third-party/pico-examples/LICENSE`), copied unchanged with each file's header. The two Pico SDK headers used come from your own pico-sdk checkout (BSD-3-Clause). The MSPM0 SDK and TI Arm Clang are TI's, installed by you under their own terms.
