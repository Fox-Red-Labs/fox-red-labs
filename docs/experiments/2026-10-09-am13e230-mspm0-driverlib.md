<!-- experiment
id: E148
board: LP-AM13E230
date: 2026-10-09
status: works
reached: verified
line: Three MSPM0 driverlib programs run on the LP-AM13E230: a GPIO toggle, a UART print and a timer PWM. All 53 lines of app code are unchanged, with a 13-line shim header. The PWM program needed the MSPM0 generated init copied with 1 line removed, because the AM13 SysConfig has no module for a timer in PWM mode.
stop: none
-->
# Does MSPM0 driverlib code run on the LP-AM13E230 with only a shim header?

**Short answer.** Yes, for these three programs. A GPIO toggle, a UART print and a timer PWM written for the LP-MSPM0G3507 build and run on the LP-AM13E230 with every line of their app code unchanged (53 lines of code in all). The shim is a 13-line `ti_msp_dl_config.h`. The reason is that the AM13 driverlib already carries about 530 macros that redirect MSPM0 names to its own (`DL_UART_Main_*`, `DL_TimerG_*` and the I2C names). What did not carry over is the part SysConfig writes: pin and peripheral setup, and for the PWM program the whole timer setup.

Terms: [vocab](../vocab.md) (DriverLib, SysConfig, shim). A shim header is a small file that makes one chip's names resolve on another. Board page: [LP-AM13E230 README](../../am13e230/README.md), [FAQ](../../am13e230/FAQ.md). Files: `portability/shim/mspm0_on_am13e230/`. Run 2026-10-09 on macOS 15.7 (Intel) with the AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and MSPM0 SDK 2.11.00.07.

**Question.** A builder with MSPM0 code in hand wants to know how much of it moves to the AM13E230, which has the same driverlib style. An earlier check of this board compared function names only: 51 of the 70 MSPM0 GPIO functions exist on the AM13 under the same name. Does code that uses them build and run?

**Vendor position.** The AM13 driverlib headers say it in comments. `dl_unicommuart.h`, for example, carries `/*! Redirects to @ref DL_UART_Config */ #define DL_UART_Main_Config DL_UART_Config`, 182 such lines for the UART, 113 in `dl_timerg.h`, 124 and 111 for the two I2C roles. No page in the SDK user guide describes them. The 26.01.00.03 release notes list "GP Timer in PWM mode" under "Drivers and Sysconfig support" in "What Is Not Supported". Checked 2026-10-09.

**What was tried.** Three MSPM0 programs, built from the terminal by `build.sh` with the SDK's prebuilt libraries (same flags as the other AM13 builds here):

1. **GPIO toggle.** `mspm0g3507/src-g3507/gpio_toggle_output.c`, the LaunchPad blink that rotates three LEDs.
2. **UART print.** The MSPM0 SDK's `uart_rw_multibyte_fifo_poll.c`, which sends `MSP!`. The folder `mspm0g3507/` holds only the GPIO program as plain driverlib code, so two MSPM0 SDK programs stand in for UART and PWM.
3. **Timer PWM.** The MSPM0 SDK's `timx_timer_mode_pwm_edge_sleep.c`: two outputs from one timer, duty 75 and 50 percent.

For each, the AM13 side is a SysConfig file that names things the way the MSPM0 one did (`GPIO_LEDS`, `USER_LED_1`, `UART_0`), so the macros the app uses come out with the same names. The three LED pins sit on one port, because the MSPM0 generator names one port per group and the AM13 generator names one per pin.

**How far it got.** Run, all three.

| Program | Proof |
|---|---|
| GPIO toggle | The LED output register (PA5, PA6, PA7) read 13 times over the probe: only one of the three bits set at a time, cycling 0x20, 0x40, 0x80. The program blinks LED4 (PA5) first, as the MSPM0 one does its first LED |
| UART print | `MSP!` arrived on the backchannel serial port at 9600 baud. The program then waits for four received bytes with nothing connected, as its source does without a loopback wire |
| Timer PWM | A sampler in the same image read the two pins 400k times: PA3 high 74.9 percent, PA4 high 49.9 percent, against 75 and 50 in the MSPM0 SysConfig. 780 cycles in 400k samples at 100 kHz gives 195 Hz, which is 100 MHz / (256 x 2000) |

**Counts.** Lines of code, comments and blank lines left out.

| Program | App code lines | Unchanged | Edited | Shim header | AM13 SysConfig |
|---|--:|--:|--:|--:|--:|
| GPIO toggle | 21 | 21 | 0 | 13 | 19 |
| UART print | 23 | 23 | 0 | 13 (same file) | 32 |
| Timer PWM | 9 | 9 | 0 | 13 (same file) | 7 |

The shim header's 13 lines hold 6 that matter: the include of the AM13 header, `CPUCLK_FREQ`, `delay_cycles`, and a 3-line `GPIO_LEDS_PORT` alias. The PWM program also has `pwm_init.c`: 36 lines copied from the MSPM0 generated config, of which 35 are unchanged and 1 is removed, plus 14 lines the AM13 generator would have written (power, pin mux, `SYSCFG_DL_init`), and a 10-line `board_pins.h` naming the timer and its pins. The sampler (`probe.c`, 18 lines) is test equipment, not part of the port.

**The line.** None reached in the app code. Four places where the MSPM0 side and the AM13 side differ, each handled once:

1. **No SysConfig for a timer PWM.** The AM13 generator offers GPIO, UART, ADC, MCPWM and timer capture and compare modules, but none for a general-purpose timer as PWM. The driverlib calls work (the PWM ran), so the MSPM0 generated init was copied. **Tool or docs.**
2. **One struct field is missing.** The field `.isTimerWithFourCC = false` in the MSPM0 PWM config does not exist in the AM13 `DL_Timer_PWMConfig` (the AM13 timers have two channels). It is the one removed line, and a shim header cannot add a struct field. **Chip limit.**
3. **Pin names differ.** MSPM0 `IOMUX_PINCM34` is AM13 `IOMUX_PINCM_PA3`; the pin function names differ too (`IOMUX_PA3_TIMG4_0_CCP0`). These come from the generator for GPIO and UART, and by hand for the PWM pins. **Tool or docs.** Look the AM13 names up in the generated `ti_msp_dl_config.h` or the device header.
4. **Port naming.** One `GPIO_LEDS_PORT` per group on MSPM0, one per pin on AM13. **Tool or docs.**

**Where it stopped.** It did not stop. Not covered: I2C, SPI, ADC, DMA and interrupt code. The ADC is the likely break: the AM13 has 12-bit converters with a different sequencer API (the release notes rename `DL_ADC_TRIGGER_ADC0_SEQ1` in this very release).

- All 270 LP-MSPM0G3507 SDK driverlib examples against the shim, as an earlier sweep did for the CC2340R5, to get a rate instead of three programs.
- The reverse: AM13 driverlib programs on an MSPM0.
- Running the copied PWM init with a four-channel timer to see whether the removed field matters on MSPM0.
- The I2C redirects (235 macros) with a sensor attached.

**Ask.** Would a team move an MSPM0 product to the AM13 this way, with SysConfig redone and the app code kept? The useful answer is which driverlib families their code uses beyond GPIO, UART and timers.

**What to do next.** On this evidence the app-level driverlib calls move; the configuration does not. Plan the work as redoing the SysConfig file and the pin list, then recompiling the app. The one place that needed hand work, a timer in PWM mode, is the one place the AM13 SysConfig has no module.

## Questions

### Does an MSPM0 `DL_UART_Main_Config` or `DL_TimerG_*` call compile on the AM13E230?

Yes in these programs. **Tool or docs.** The AM13 driverlib headers redirect about 530 MSPM0 names with `#define` lines, for example `#define DL_UART_Main_Config DL_UART_Config`. No SDK user guide page lists them.

### Why is there no SysConfig module for a timer in PWM mode?

The AM13 generator has none. **Tool or docs.** The release notes list "GP Timer in PWM mode" as not supported. The driverlib calls work: copy the MSPM0 generated init and remove `.isTimerWithFourCC = false`.

### Why does `.isTimerWithFourCC` fail to compile?

The AM13 `DL_Timer_PWMConfig` has no such field, because its timers have two channels. **Chip limit.** A shim header cannot add a struct field, so delete the line.

### Why does `GPIO_LEDS_PORT` not exist?

The AM13 generator names one port per pin, MSPM0 one per group. **Tool or docs.** This repo's 3-line alias in the shim header maps it.
