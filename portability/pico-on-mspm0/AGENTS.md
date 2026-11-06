# Raspberry Pi Pico SDK examples on an MSPM0 LaunchPad: notes for AI coding assistants

That the example's `.c` file stays untouched and everything else is this folder: Pico SDK headers of its own, first on the include path, over TI DriverLib. Five pico-examples ran this way on an LP-MSPM0G3519. Each line below comes from those runs or from a file in this folder.

Terms: [docs/vocab.md](../../docs/vocab.md) in the Fox Red Labs repo. In short: **Pico SDK** is Raspberry Pi's C SDK; **DriverLib** is TI's C library for the MSPM0 peripherals; **SysConfig** is TI's tool that writes the pin and clock setup code; **DSLite** is the command-line flasher inside UniFlash. 1 kB = 1,024 bytes. Checked 2026-10-25 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS -Os, SysConfig 1.28.0, pico-sdk 2.3.1 and pico-examples sdk-2.3.1, on one LP-MSPM0G3519 with the Waveshare 3.5 inch panel fitted.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `portability/` or `ports/` are in the Fox Red Labs repo; they are not in your copy.

## The layer

| Part | Fact |
|---|---|
| What it covers | `pico/stdlib.h`, `pico/time.h`, `hardware/gpio.h`, `uart.h`, `i2c.h`, `timer.h`, `irq.h`, `pwm.h`, `adc.h`, only the calls five examples make (`portability/pico-on-mspm0/README.md`) |
| Size | 316 layer lines plus 81 board lines; all count as written code (`portability/pico-on-mspm0/README.md`) |
| Real SDK headers used | `pico/error.h` and `pico/binary_info.h`, from a pico-sdk 2.3.1 checkout, found after the layer's `include/` (`portability/pico-on-mspm0/build.sh`) |
| Start-up | SysConfig init, SysTick and the board start-up run as a C constructor before the example's `main()`; the board start-up takes 0.4 s (`src/runtime.c`) |
| Clock | 32 MHz from the internal oscillator; PWM dividers divide 32 MHz, not the Pico's 125 MHz (`boards/mspm0g3519/board.syscfg`, `include/hardware/pwm.h`) |
| Pins | Pico GPIO 0/1 = PA10/PA11 (UART0, probe serial port), 4/5 = PC3/PC2 (I2C2), 25 = PB27 (green LED, TIMA1 compare 1), 26 = PA27 (ADC0 channel 0); other numbers are ignored (`boards/mspm0g3519/pins.c`) |
| Stack | 2 kB, the Pico default; the generated linker file asks for 256 B (`build.sh`) |
| Sizes | 1.8 kB (`hello_serial`) to 10.6 kB (`bus_scan`) of flash; 2.1 to 2.4 kB of RAM with the stack (`ports/pico-sdk/README.md`) |

## Start here

| Step | Command |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, a pico-sdk checkout (`portability/pico-on-mspm0/README.md`, Set up once) |
| Build | `SDK=<sdk> CGT=<compiler> SYSCFG=<sysconfig_cli.sh> PICO_SDK=<pico-sdk> OUT=<build> pico-on-mspm0/build.sh <pico-examples>/blink_simple` (ran outside the repo with only the listed files, 2026-10-25) |
| Check the image | `python3 tools/preflight_image.py mspm0g3519 <build>/blink_simple.out` |
| Back up | the Back up row of `mspm0g3519/AGENTS.md` (DSLite reads of main flash, data flash and NONMAIN, each read twice and compared) |
| Flash | `<UniFlash>/dslite.sh --mode flash -c MSPM0G3519_<probe serial>.ccxml -e -f -v <build>/<example>.out`, then press nRST |
| Console | Open `/dev/cu.usbmodem<probe serial>1` at 115200 first, then press nRST, so the banner is not missed |
| Restore | the Restore row of `mspm0g3519/AGENTS.md` |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Link error `symbol "puts" redefined` in `libc.a<fputs.c.obj>` | The C library keeps `puts` and `fputs` in one object | Replace `fputs` as well as `puts`, `putchar` and `printf` (`src/stdio_uart.c`) | The image links; no debugger console code in the map |
| `--stack_size` on the link line has no effect | The generated `device_linker.cmd` sets 256 B and wins when it comes later | Put `-Wl,--stack_size=0x800` after `device_linker.cmd` (`build.sh`) | `__STACK_SIZE` reads 0x800 in the map |
| `pwm/led_fade` compare value stays 0 | A timer reset in `pwm_init` cleared the wrap interrupt the example enabled first | Power and reset a slice on its first use only (`src/pwm.c`) | Compare 1 reads changing perfect squares |
| `tools/check/blink.py` FAIL with PB27 changing | It reads the output-enable bit right after its own reset, inside the 0.4 s start-up | Read `0x400A32C0` (GPIOB DOE) over DSLite `memory` without a reset | Bit 27 is 1 |
| `bus_scan` finds nothing at 0x38 | The FT6336U keeps the bus busy for about 65 ms after its reset | Keep the 5 ms low, 400 ms wait in `pico_board_init` (`boards/mspm0g3519/pins.c`) | Only 0x38 answers, 3 of 3 runs |
| An I2C read leaves the bus busy | The controller's ACK bit set on a read acknowledges the last byte | Start reads with `DL_I2C_CONTROLLER_ACK_DISABLE` (`src/i2c.c`) | The next transfer starts |

## Rules for the assistant

1. Never edit the pico-examples `.c` file; if a call is missing, add it to the layer and count its lines.
2. Add only the calls a program makes. A Pico GPIO number not in `pins.c` does nothing, so add its row before using it.
3. After a clock change in SysConfig, read `CPUCLK_FREQ` in the generated `ti_msp_dl_config.h`; the time base, I2C speed and PWM rate are computed from it (`src/time.c`, `src/i2c.c`).
