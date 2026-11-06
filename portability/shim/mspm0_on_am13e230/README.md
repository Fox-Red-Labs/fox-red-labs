# MSPM0 DriverLib programs on the LP-AM13E230

Short answer: `./build.sh gpio`, `./build.sh uart` and `./build.sh pwm pwm/pwm_init.c pwm/probe.c` build three MSPM0 programs for the AM13E230 with the app code unchanged. The only shim is `ti_msp_dl_config.h`, a 13-line stand-in for the header SysConfig writes on the MSPM0. What ran and how it was checked: [the experiment page](../../../docs/experiments/2026-10-09-am13e230-mspm0-driverlib.md).

Terms: [vocab](../../../docs/vocab.md). Board: [LP-AM13E230 README](../../../am13e230/README.md), [FAQ](../../../am13e230/FAQ.md). Checked 2026-10-09 with the AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and MSPM0 SDK 2.11.00.07.

| Folder | MSPM0 source (unchanged) | AM13 side |
|---|---|---|
| `gpio/` | `mspm0g3507/src-g3507/gpio_toggle_output.c`, the LP-MSPM0G3507 blink that rotates three LEDs | `example.syscfg`: four output pins on one port |
| `uart/` | MSPM0 SDK `uart_rw_multibyte_fifo_poll.c`, sends `MSP!` | `example.syscfg`: UART on the backchannel pins PA0 and PA1, 9600 baud |
| `pwm/` | MSPM0 SDK `timx_timer_mode_pwm_edge_sleep.c`, two PWM outputs | `pwm_init.c` (the MSPM0 generated PWM init, copied, one line removed), `board_pins.h`, `probe.c` (test harness that counts how often each pin reads high) |

The AM13 SysConfig has no module for a general-purpose timer in PWM mode, so the PWM program carries the MSPM0 generated init instead of a `.syscfg` description. **Tool or docs.**

If the PWM pins read 0 percent high with a working timer, the pins' input buffers are off, which is how a peripheral output pin is set up. `probe.c` turns them on. **Setup or handling.**

If `printf`-style output from `uart/` does not appear in a line listener, it has no newline: run `RAW=1 am13e230/tools/flash_listen.sh <image> 8 9600`.

To check a run: gpio with `am13e230/tools/pin_check.sh 0x400F1280 5 9 0.0`, uart with the listener above, pwm with `am13e230/tools/run_image.py <image> 12 probe:20`.
