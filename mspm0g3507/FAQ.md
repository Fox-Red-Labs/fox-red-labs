# LP-MSPM0G3507 FAQ: STM32 and Arduino ports, touch display, Zephyr and benchmarks

Four topics on this page: (1) porting STM32 code to MSPM0 with TI's migration skill, (2) running Arduino code and libraries on the LP-MSPM0G3507, (3) driving the Waveshare 3.5inch Capacitive Touch LCD (screen, touch chip, LVGL graphics library, jumper wires) from the LP-MSPM0G3507, and (4) writing one app with DriverLib, TI Drivers and Zephyr.

Every problem seen moving STM32CubeG0 examples to the TI LP-MSPM0G3507 LaunchPad development kit with TI's migration skill, running Arduino libraries on it, and driving the Waveshare 3.5inch Capacitive Touch LCD (ST7796S screen driver, FT6336U touch chip) from it, on 2026-10-06. Apple Silicon was not tested. Each answer starts with the fix, names the cause, and quotes the exact error text where it was saved. Versions: Intel Mac, macOS 15.7, CCStudio 21.0.1.00010, MSPM0 SDK 2.11.00.07, SysConfig 1.28.1 for the STM32 ports (1.26.3 for the Arduino layer), TI Arm Clang 5.1.1.LTS. 1 kB = 1,024 bytes on this page.

The write-ups behind these answers: STM32 port evaluation and [Arduino compatibility layer](../portability/arduino/README.md), with its pin map and library versions in arduino-compat/DETAILS.md. Words used here are in [docs/vocab.md](../docs/vocab.md). Display and LVGL write-ups: [display-test](display-test/README.md) and [LVGL on the G3507](lvgl-g3507/README.md). The display module carries a TXS0108E level translator (a chip that converts signal voltages) between its header and the screen.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | A mistake made during these runs. Listed so you do not repeat it |
| Setup or handling | How the hardware or the workspace was set up |
| TI example default | A TI example's default works on TI's bench but not for this job |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The silicon cannot do it |
| Third-party behavior | ST code, Arduino libraries, OpenOCD, Claude Code, macOS: software that is not TI's |
| Unknown | Not found. "Unknown (suspected: ...)" names the lead |

Numbers seen on the board over the debugger or the serial port on 2026-10-06 were not saved to a file unless an entry names one. Button presses in those tests were made by the debugger driving the button pin, not by a finger.

Contents: [Running TI's migration skill](#running-tis-migration-skill), [What the skill changes in your code](#what-the-skill-changes-in-your-code), [Building and running the ported projects](#building-and-running-the-ported-projects), [The Arduino layer](#the-arduino-layer), [Display: wiring and power](#wiring-and-power), [The screen](#the-screen), [The touch chip](#the-touch-chip), [LVGL](#lvgl), [Flashing and the Mac](#flashing-and-the-mac), [DriverLib, TI Drivers and Zephyr](#driverlib-ti-drivers-and-zephyr), [Checks that mislead](#checks-that-mislead).

## Getting started: the first blink

### Which Claude plan is enough for the blink steps, and what does it cost?

Not tested: no plan was compared. The steps use Claude Code's ordinary chat and tool calls, nothing plan-specific; Anthropic's own pricing page is the place to read current plans and prices, and CCStudio's AI panel also accepts an API key instead of a subscription (TI's CCStudio AI guide, read 2026-10-09).

### Can you blink the LP-MSPM0G3507 without an AI assistant, or with an assistant other than Claude Code?

Yes without one: in CCStudio, import TI's `gpio_toggle_output` example, build it and press Debug, then Resume, as TI's own guide says; or from a terminal, build it with the SDK makefile and flash it with OpenOCD, the route timed on the LP-MSPM0L1306 (3.3 s build, 0.6 s flash). Only Claude Code was tried; TI's guide names Codex as the other assistant it tested, and Codex was not run. Source: `mspm0l1306/README.md`, "First blink from a terminal".

### Does the blink recipe work on an Apple Silicon Mac, on Windows or on Linux?

Every step was timed on one Intel MacBook on macOS 15.7.9. TI's download page offers CCStudio 21.0.1 for all three systems and the MSPM0 SDK for each; on Apple Silicon CCStudio runs through Rosetta 2, which Apple says it stops supporting fully after macOS 27 (`mspm0g3507/README.md`). The paths in step 6's server file are macOS paths and change on the other two. Source: `site/data/start-here.json`, "also needed".

### The Mac sees nothing after the LP-MSPM0G3507 is plugged in. Is the cable charge-only?

Try another micro-USB cable: a charge-only cable looks the same and the board will not appear. With a data cable the Mac shows an XDS110 debug probe and two serial ports with no driver to install; `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` lists the probe. A USB-C-only Mac also needs an adapter: an LP-MSPM0G3507 box held a cable with a USB-A plug on the computer end.

### The LP-MSPM0G3507 started blinking as soon as it was plugged in, before anything was done. Why?

That is the program already on the board: TI's out-of-box demo on a new board, or a previous owner's program on a used one. It shows the board has power, not that your tools work, which is why the recipe's first prompt changes the blink instead of only loading one. A used board can be blinking another user's program on plug-in.

## Running TI's migration skill

### Where is TI's STM32 to MSPM0 migration skill, and why is it not in the CCStudio AI docs?

It is in the CCStudio install at `/Applications/ti/ccs2101/ccs/Code Composer Studio.app/Contents/Resources/ai/skills.alpha/ti-ccstudio-migrate-to-ti`. Copy that folder into `<CCStudio workspace>/.claude/skills/`. The skill is marked "EVALUATION USE ONLY". Checked 2026-10-06 on CCStudio 21.0.1.

### A headless run stops with "Claude requested permissions to use mcp__ccs-project__changeSysConfigVersion, but you haven't granted it."

Leave CCStudio's `.claude/settings.local.json` out of the run, or run the skill in an interactive run and approve the prompt. CCStudio writes that settings file with the tool on the "ask" list, while the skill's own instructions say to call it without asking for a fresh project. A headless run treats "ask" as denied, even in bypass mode.

### The skill's SysConfig step fails on `<<<<<<< HEAD` in its own instructions

Delete that line. It is line 92 of `skills.alpha/ti-ccstudio-migrate-to-ti/agents/sysconfig-configurator.md`. The file ships with a git merge-conflict marker and no partner marker. In the run, the orchestrator edited its copy and retried; the file in the CCStudio install is unchanged.

### A run ended with `[Request interrupted by user]` and nobody pressed anything

Resume it with the skill's resume prompt: `/ti-ccstudio-migrate-to-ti Resume the migration of <path> to MSPM0G3507 ...`, with CCStudio open. A chained unattended script that is killed or times out ends a delegated run this way. Resuming built both interrupted projects, one in 8.1 minutes and one in 33.4 minutes.

### A resumed run says the plan belongs to a different migration

Point it at the backup named `migration-plan-before-<next run>.md`, which holds the earlier plan. The skill keeps one active `migration-plan.md` at the workspace root, and a later migration overwrites it. A resume can find the right plan in a backup file, use it, and put the root plan back. Keep one workspace per migration.

### The skill's project-initializer step sits for minutes waiting for generated files

Check that CCStudio is responsive, and resume the run if it stops. In one run the initializer's second tool call took 3.7 minutes, and it then polled for 5 minutes for a file that never appeared, before the run was interrupted. The resumed run imported the project without trouble. Two other runs finished the whole import in 5.5 and 4.4 minutes. Whether CCStudio was busy or stuck was not checked. Source: `stm32-port-eval/03-gpio-exti/README.md`.

## What the skill changes in your code

### The port moved the pins: PA4 became PA27, PA11 became PA12, the LED moved from PA5 to PA0

Read the report's pin table and rewire. The skill chooses pins instead of translating them, and logs the change in the report. In port 04 the ADC input moved from PA4 to PA27 with no prompt. In port 05 channel 4 moved from PA11 to PA12 because PA11 cannot carry that timer output on this package. In port 02 the plan said to keep PA2 and PA3 for the serial port, but neither is a valid UART pin on the LQFP-64(PM) package, so the configurator used the kit's USB-serial pins PA10 and PA11.

### A ported button interrupt never fires on the LaunchPad

Check which way your button pulls. The skill copied ST's active-low setup (internal pull-up, falling edge) onto S1 (PA18), which on the LP-MSPM0G3507 is active-high, so the pin already rests high and a press cannot make a falling edge. In port 03, driving S1 low toggled LED1 each time, and driving it high toggled nothing. Port 04 kept the same logic. The fix, tested in both ports: set PA18 to pull-down and rising edge in SysConfig (`stm32-port-eval/03-gpio-exti/fixed/fix.diff`); after it, each active-high press toggled LED1. S2 (PB21) is active-low and matches ST's setup, which is why port 02's button worked. Seen 2026-10-06 over the debugger, not saved.

### The CPU clock changed from 56 MHz to 32 MHz, or differs from port to port

Set the clock yourself in SysConfig if timing matters. Port 01's planner removed the clock setup under a rule of "don't configure the clock unless the source needs a specific frequency", so the chip ran at its 32 MHz default instead of 56 MHz. Ports 02 to 05 reproduced 56 MHz. Each reason is written down, but the rule is not the same from port to port.

### The skill dropped part of the program, and the report says "None identified"

Read the end-of-run summary and diff the source. The skill only makes one-to-one API swaps. In port 04 it deleted the code that turns counts into millivolts and degrees. In port 05 it deleted the error LED. In port 02 a fixed 100 microsecond delay replaced the wait on the transmit-empty flag. The report's "Unsupported features" table says "None identified" for all three.

### A ported button handler would fire several times for one press of S2

Add a software debounce of 200 to 300 ms: ignore further edges for that time after the first. The port 02 report says S2 bounces and needs that debounce, and says it was not implemented, because the skill allows only one-to-one API replacements. ST's source has none either, so the port adds none. Nobody pressed S2 with a finger, so no double messages were seen. Source: `stm32-port-eval/02-usart-tx-vcp/migration-report.md`.

### A ported ADC reads the supply, not ST's internal reference (`VREFINT`)

Treat that channel as a supply reading, and add a real reference measurement yourself if you need one. The skill found no one-to-one MSPM0 channel for ST's `VREFINT` (an internal reference of about 1.2 V), so it used channel 15, the supply monitor, with a 1.4 V reference (SysConfig's default), and flagged this for review in the report. That channel measures VDD / 3, so counts are not comparable with ST's. Port 04 read 3,210 counts, which is 3.29 V by 3,210 / 4,095 x 1.4 V x 3. Not checked: the data sheet's channel list. Seen 2026-10-06, not saved.

### The ported file still has STM32 comments, and the report contradicts itself

Ignore them or delete them. Port 01 keeps ST's comment "STM32G0xx HAL library initialization ... Systick timer is configured by default" above `SYSCFG_DL_init()`, which does neither. The report's memory row reads "RAM | 36 KB | 32 KB | 🔴 Low" for a program that uses 512 bytes, because it compares the two chips' totals. Port 04's report says in one row that the build "will fail" and in another that it passed.

### The report says "hardware testing pending". Does the port work?

Not known until you run it. The skill ends at "build verified" and never flashes. Its companion skill `ti-ccstudio-test-migration` would test on hardware; it was not run, and the ports were tested by hand instead.

### TI's driver header says the 56 MHz bus clock "should always remain under 40MHz". Is the generated clock setup out of spec?

Not shown either way. Ports 02, 03, 04 and 05 set `DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1)` at a 56 MHz CPU clock, which puts the UART and timer bus clock at 56 MHz. The port 02 UART printed correctly at 115,200 baud, and the port 05 channel 4 duty cycle measured near the expected value (a duty cycle does not show the frequency). That is partial evidence, not proof. Not checked: the data sheet limit for those peripherals, or a measured frequency.

### The skill could not read the LED and button pins

Clone the ST repository with its submodules. The run cloned only the examples, without `Drivers/BSP/STM32G0xx_Nucleo/`, so the analyzer could not read ST's `LED4_PIN` and button definitions and fell back to defaults (LED1 on PA0, S1 on PA18). Ports 03, 04 and 05 were affected.

## Building and running the ported projects

### The first build fails with `fatal error: 'main.h' file not found`

Add `-I"${PROJECT_ROOT}/Inc"` to the compiler's include path. The project initializer copies the whole ST project, including ST's interrupt, clock and C-library stubs, and does not put `Inc/` on the include path. The build agent fixed this in every port, so on an STM32CubeIDE project the first build always fails. Seen in ports 01 to 05.

### The build fails with `error: unknown type name 'uint32_t'` in `Src/system_stm32g0xx.c`

Add `#include <stdint.h>` to that file. The skill removes ST's `stm32g0xx.h` include, which had pulled in `stdint.h` indirectly, and does not add the standard header back. The port 03 build verifier made this fix on its second attempt. Source: `stm32-port-eval/03-gpio-exti/migration-report.md`.

### `make` in `Debug/` stops with `invalid option -- O`

Use the `gmake` that ships with CCStudio, at `ccs/utils/bin/gmake`. The generated makefile needs GNU make 4, and Apple's `make` is older. `gmake -B all` built ports 04 and 05 from a terminal with the same output as the IDE build.

### The ADC port converts once, then stalls: DMA never moves the results and later button presses do nothing. What is the fix?

Make three corrections to the skill's output in `main.c` and the SysConfig file. All three came from what the skill generated. (1) The DMA source was the ADC's FIFO address (`DL_ADC12_getFIFOAddress`), but FIFO mode is never enabled, so nothing is written there. Use the result registers: `DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) DL_ADC12_getMemResultAddress(ADC12_0_INST, ADC12_0_ADCMEM_0))`, and in SysConfig set the DMA address mode to block-to-block (`b2b`, so the source address steps up), source width WORD (the result registers are 32 bits) and destination width HALF_WORD. (2) The generated start-up code calls `DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 0)`, which is zero results per DMA trigger, so DMA moved nothing. Call `DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1)` before starting the DMA channel; TI's own SDK examples use 1 or 6 (MSPM0 SDK 2.11.00.07, `examples/nortos/LP_MSPM0G3507/driverlib/adc12_14bit_resolution` and `adc12_max_freq_dma`). (3) The skill kept ST's active-low button logic on S1 (PA18), which is active-high on this LaunchPad. Set PA18 to pull-down with a rising-edge interrupt (see the button question above). What the skill's version did: at start-up the ADC converted all three channels once, as its result registers (`MEMRES`) showed over the debugger, because the DMA buffer in RAM stayed at its starting fill value. The supply-monitor register read 3,210 counts, which is 3.29 V by 3,210 / 4,095 x 1.4 V x 3 (that channel measures VDD / 3). After that the DMA and ADC interrupts never fired (checked with breakpoints), and the ADC stayed disabled (`CTL0` = 0, the ADC's control register). That run was observed 2026-10-06 and not saved. With the three corrections the project builds (2.9 kB of code, 16 more than the skill's 2,904) and, with the debugger simulating presses (PA18 driven high for 40 ms, then released), the DMA buffer held 0x0000, 0x0330 and 0x0c86: channel 0 (PA27) 0 counts (what is wired to the pin was not checked), the temperature sensor 816 counts, and the supply monitor 3,206 counts, which is 3.29 V by the same arithmetic. A hardware breakpoint on `HAL_ADC_ConvCpltCallback` was hit, so the DMA done callback runs. One difference from ST's example remains: the MSPM0 sequence converts all three channels every time it starts, where ST's example converts one channel per button press in discontinuous mode. The count-to-millivolt and count-to-degrees code is still deleted, and nobody pressed S1 by hand. Source: `stm32-port-eval/04-adc-multichannel/fixed/fix.diff` (against the skill's version).

### The PWM port and a serial program both want PA10. Can they run together?

No, not as ported. Port 05 puts PWM channel 2 on PA10, which on the LP-MSPM0G3507 is also the USB-serial transmit path (jumper J21, the jumper that routes PA10 to the XDS110 serial chip, was up during the test), so anything on PA10 reaches the serial bridge. Port 02's serial port (UART0 on PA10 and PA11) uses the same pin. Move one of them to another pin in SysConfig; this was not run. Source: `stm32-port-eval/05-tim-pwm/README.md`.

### The report gives the PWM frequency as 24390.24 Hz. Is that exact?

Treat it as about 24,390 Hz, possibly 24,380 Hz, until it is measured. The report divides 56 MHz by 2296, the SysConfig field. The `LOAD` register read 2295 on the board, and a down-counting timer from 2295 to zero is 2,296 ticks, which matches. If the hardware spends one more tick at the reload, the period is 2,297 ticks. The difference is 0.04%. Not checked: the TI reference manual, or a frequency measured with a scope or logic analyzer.

## The Arduino layer

### `fatal error: 'Print.h' file not found` when building Arduino_GFX

Add ArduinoCore-API's `api/deprecated/` folder to the include path, and `api/deprecated-avr-comp/` for `<avr/pgmspace.h>`. `Arduino_GFX.h` includes `<Print.h>`, a legacy top-level header that only old cores provide. ArduinoCore-API ships wrappers in those two folders.

### The compiler says an exception specification does not match for `atexit`

Build with `-DHOST`. `api/Common.h` declares `int atexit(void (*)()) __attribute__((weak))`, and TI's `stdlib.h` declares it with a C++14 exception specification. `-DHOST` makes the header skip its own declaration. The exact compiler text was not saved.

### A warning about `main` being declared `extern "C"`

Build with `-Wno-main`. The same ArduinoCore-API header declares `main` that way. As a side effect `main` is weak, which changes nothing here.

### `SPIClass` is ambiguous

Name your class something else and derive from `HardwareSPI`. The first `SPI.h` defined its own `SPIClass`, and ArduinoCore-API already has `typedef HardwareSPI SPIClass` in namespace `arduino`. The class is now `MspSPI`. Deriving from `HardwareSPI` forces four interrupt methods this layer does not support; they stop at a breakpoint (`compat_unsupported`) instead of pretending to work.

### Adafruit_BusIO does not compile: no `digitalPinToPort`, `portOutputRegister`

Define the AVR-style port macros in `Arduino.h`. `Adafruit_BusIO`'s software-SPI file expects them on any ARM board. The layer's macros are backed by `DOUT31_0` and `DIN31_0`. Neither sketch uses that code path, so the macros have not run.

### A function in a sketch is "not declared" although the Arduino IDE accepts it

Generate the prototypes from the preprocessed sketch. Arduino sketches rely on the IDE adding function prototypes. The first `tools/ino2cpp.py` read the raw text and missed `serialOut()`, whose signature sits inside an `#ifdef`. It now runs the preprocessor first, as the IDE does.

### The link fails on the compiler builtins, or the program runs out of stack or heap

Link with `-llibclang_rt.builtins.a`, and give the program a 2 kB stack and a 4 kB heap. The TI linker takes `-l` plus a file name, so `-lclang_rt.builtins` does not find it. SysConfig's generated linker file asks for only a 512-byte stack and no heap, too small for C++ and `new`. `build.sh` copies the file to `out/linker.cmd` with the larger sizes.

### The screen shows noise, or nothing, with Arduino_GFX's default SPI mode

Ask for SPI mode 0, as the sketch's display header does. For a board it does not know, `Arduino_HWSPI` picks SPI mode 2, and `Arduino_ST7796::begin` sets a mode only for a few boards. The module's ST7796S is specified for modes 0 and 3. `pdqgfx_libmode`, the build that keeps mode 2, was not run, so it is not known whether mode 2 fails on this module.

### Arduino_GFX starts at 24 MHz SPI, faster than the panel allows

Define `F_CPU` in `variant.h`. `SPI_DEFAULT_FREQ` is 24 MHz on unknown boards unless `F_CPU` is defined, and the panel's limit is 15 MHz. With `F_CPU` set to 32000000UL the library uses `F_CPU / 4`, 8 MHz.

### OpenOCD reports a CRC mismatch after `program <file> verify`, but the program runs

Leave `verify` off the command. On these images `program ... verify` reported a CRC mismatch (exact text not saved), while programming without it and running worked. OpenOCD's quick checksum check also fails on other MSPM0 images, then falls back to a full read-back that passes; see the OpenOCD entry in [mspm0l1306/FAQ.md](../mspm0l1306/FAQ.md). These images were not read back, so a real mismatch is not ruled out.

### `micros()` is 1,725 microseconds ahead of `millis()` in `shimcheck`

Probably nothing: read both counters together before you print. One reading, not saved, printed `millis=1000 micros=1001725`, a 0.17% gap. The sketch reads `millis()`, then prints about 19 characters before it reads `micros()`. At 115,200 baud that print takes about 1.65 ms, close to the gap. Not checked: both counters read back to back, or against an outside clock.

### Holding S1 does not stop the LED blinking in `shimcheck`

No fix was run, and the sketch as written cannot show an S1 press. S1 is active-high on this LaunchPad, but the sketch sets S1 to `INPUT_PULLUP` and toggles the LED only while S1 reads HIGH, so it stops only if S1 reads low. Unpressed reads high (pull-up), and pressed should also read high, so the LED keeps toggling either way. Measured: the unpressed readings were `S1=1 S2=1`. Not measured: S1 pressed. A fix to try, not run: set S1 as a pull-down input and stop the LED when it reads high, or test S2 (active-low) instead.

### Arduino_GFX fills the screen in 0.51 s, and the hand-written driver does it in 0.365 s

Not fixed. Five fills took 2.55 s at 8 MHz SPI. The suspected cause is the layer's `SPI.transfer` loop, which runs full duplex for every byte. No version with that loop changed was compared.

### `analogRead`, `analogWrite`, `tone`, `attachInterrupt` or `Serial1` will not compile or link

Write that part with TI's DriverLib. These calls were not written, a scope choice, and whether the chip can support each was not checked. From reading the headers, not from a saved build: `analogRead`, `analogWrite`, `tone` and `attachInterrupt` are declared in ArduinoCore-API's `api/Common.h` and defined nowhere in this core, so a sketch that calls one should compile and then fail at link with an undefined reference. `Serial1` is declared nowhere, so it should fail at compile. The exact error text was not saved. The SPI class's `usingInterrupt`, `notUsingInterrupt`, `attachInterrupt` and `detachInterrupt` methods compile and stop the program at a breakpoint (`compat_unsupported`), and a pin number that does not exist stops at `compat_bad_pin`.

### `Serial.printf` does not compile

Use `Serial.print` and `Serial.println`, or `snprintf` into a buffer. ArduinoCore-API's `Print` does not define `printf`, and this layer does not add one.

### Why is a screen-drawing sketch 75.6 kB?

Expect that from Arduino_GFX. The library's classes keep every drawing method alive through their virtual tables, so 47.4 kB of `pdqgfx` is the library. The linker map shows `Arduino_GFX.cpp` and `Arduino_TFT.cpp` each carry their own copy of the 1.2 kB default font, about 1.25 kB wasted. `-Os` gave 49.7 kB. `LITTLE_FOOT_PRINT` was not run.

## Wiring and power

### Which Waveshare 3.5 inch screen is this: the capacitive touch one or the resistive one?

The capacitive one, sold as the "3.5inch Capacitive Touch LCD". Its touch chip is a FocalTech FT6336U on I2C address 0x38: the registers read vendor `0x11`, chip `0x64`, firmware `0x12` from registers A8, A3 and A6 (observed 2026-10-06; not saved). Waveshare's listing has a resistive sibling; the resistive model was not tested and is a different touch chip. `mspm0g3507/display-test/README.md`.

### Why does the picture say 3.3 V and not 5 V for the screen's VCC?

Because the kit's pins are 3.3 V, and Waveshare says the supply and signal voltage must match. The module's VCC pin takes 3.3 V or 5 V. The schematic puts the translator's chip-side voltage on a 3.3 V rail the module makes from VCC, so wiring VCC to the kit's 3V3 (J1 pin 1) keeps both sides the same. 5 V was not run, so what it does is not known.

### Which /dev/cu device is the board's serial port?

The one whose name ends in 1, for example `/dev/cu.usbmodemMG3500011` for a probe with serial `MG350001`. The kit's debug probe shows up as two serial ports. The one ending in 1 is the chip's UART (UART0, 115200 8N1); the one ending in 4 is not used here. List them with `ls /dev/cu.usbmodem*`. Source: `mspm0l1306/FAQ.md` for the same probe family.

### Which jumper wire goes where?

Use the numbered picture: display-g3507-wiring.png shows all 12 wires (13 with MISO) on the board drawing, with pin names and header pin numbers. Unplug USB before moving any wire. The two loop tests that need one wire each are DAC to ADC and the UART loopback; the PWM check loop is PA12 to PA13.

## The screen

### The screen stays dark or shows static, even though every wire is right. What is wrong?

Wait about 10 µs after chip select goes high before the next command. Measured: each command sent alone took effect, but the second of two back-to-back commands was lost, and "display on" was always the second one. A 10 µs gap fixed it, and a slower SPI clock did not. The suspect is the module's TXS0108E not passing the few-hundred-nanosecond chip-select pulse; no scope was used to check. To see it yourself, wire MISO (J2 pin 14, PB7) and read the power-mode register `0x0A` after each command: `0x9C` means awake with the display on. `cs_gap()` in `mspm0g3507/display-test/display_test.c`.

### The colors are inverted, black shows as white. How do you fix it?

Send command `0x21` (display inversion on) after sleep out. The init sends it, and its comment says this IPS panel needs it. No picture of the screen without it was saved, so the inverted state is not recorded. `CMD(0x21)` in `display_test.c` and `port.c`.

### 16-bit SPI frames are silently ignored and the picture is garbled. Why?

Switch the SPI block off, set the frame size, then switch it back on. Calling `DL_SPI_setDataSize` while SPI is enabled does nothing and returns no error, and the documentation does not say it needs the block disabled. 16-bit frames are what make the full-screen fill about 8% faster than 8-bit frames (168.1 ms against 182.5 ms at 16 MHz). `frames16()` in `lvgl-g3507/src/port.c`.

### A SysConfig clock line is ignored and the chip stays at 32 MHz when 80 MHz is requested. Why?

Remove `SYSCTL.forceDefaultClkConfig` and set the clocks through the clock tree. With that line left in, SysConfig accepted every clock setting without a warning and generated 32 MHz code. The fix used a copy of TI's `sysctl_mclk_syspll` clock tree (`display80.syscfg`), then read `CPUCLK_FREQ` in the generated `ti_msp_dl_config.h` to confirm.

### Is 40 MHz SPI safe for this panel?

It worked here, but the margin is not known. The panel's data sheet allows about 15 MHz writes (66 ns minimum cycle), so 40 MHz is about 2.7 times that. 64 of 64 pixels read back correctly at 40 MHz SPI with an 80 MHz CPU clock. Only one row was read back, the screen was not checked by eye at 40 MHz, and longer wires and a second module were not run. If the screen shows noise, use 20 MHz.

### The screen takes 370 ms to be ready after power-up. Can that be shorter?

Yes: 12 ms with the shortest waits the ST7796S data sheet allows. The usual init waits 120 ms after reset, after the software reset and after sleep out, copied from common drivers; that was 370.2 ms to a ready screen. The fast option (`-DFAST_BOOT` in `display_test.c`) measured 12.2 ms, and the power-mode register read `0x9C` (awake, display on) after it too. A first full frame then took 180 ms at 16 MHz SPI against 538 ms with the usual init. The chip itself starts in 0.26 to 0.31 ms (data sheet SLASEX6C).

## The touch chip

### The touch chip does not answer on I2C1 at address 0x38. Is it dead?

No. Drive the two pins as software I2C instead. With the I2C1 block every read of 0x38 failed: first not acknowledged, later the controller reported the bus busy. The same pins as plain open-drain GPIO at about 100 kHz got an acknowledgement and correct registers every time. The suspect is the module's translator on an open-drain bus. `display-test/README.md`, trap 2.

### Adding a pull-up to the I2C pin leaves the line never going low. What happened?

Call `DL_GPIO_enableHiZ` again after `DL_GPIO_initPeripheralInputFunctionFeatures`. That function rewrites the whole pin register and clears the open-drain setting SysConfig's code had set with `DL_GPIO_enableHiZ`. Fixing it alone did not make the I2C block work on this module.

## LVGL

### LVGL on a 2 kB stack shows `stack_free=0` or behaves oddly. What stack does it need?

Use 6 kB. The first build used 2 kB and the paint check read `stack_free=0`. The firmware paints the stack with `0xA5A5A5A5` at boot and counts the untouched bytes. With 6 kB, 2.1 kB stayed untouched on both builds, so the measured peak is 3.9 kB. Nobody used the touch screen in those runs, so the peak does not cover the button, slider and full-redraw paths.

### Does LVGL fit in 32 kB of RAM?

Yes, trimmed: 25.1 kB of RAM with a 12 kB heap, 6 kB stack and a 6.2 kB draw buffer. Flash is the tight resource: the demo uses 125.4 kB of 128 kB at 32 MHz (126 kB at 80 MHz) after removing a font and several drawing features, and the same demo with a second font needs 146.7 kB. Other trims and an older LVGL were not run. `lvgl-g3507/README.md`.

### What had to be cut for LVGL to fit in the 128 kB flash?

A second font and several drawing paths. The full demo with Montserrat 20 added is 146.7 kB against 128 kB of flash. Dropping Montserrat 20 saves 21.3 kB. The size patch (`size-patch.diff`, switched on by `LV_DRAW_SW_SLIM` in `lv_conf.h`) removes shadow, blur, layer, triangle, mask-rectangle and image drawing and the image decoder, and saves 19.7 kB with the font in both builds. Anything that draws an image, a shadow or a layer draws nothing for that part. An older LVGL, other trims and another graphics library were not run. `lvgl-g3507/README.md`, "What did not fit".

### The serial stats line from the LVGL demo is cut off in the middle. Is the serial link dropping characters?

No, the demo's own code cut it. The firmware built each line in a 160-byte buffer (`static char line[160]` in `lvgl-g3507/src/app.c`) and the line is 176 to 179 characters. The buffer is now 256 bytes, and the two runs with the full line (32 MHz and 80 MHz) hold 19 complete lines each. The older logs without it are cut at the end of each line.

### The LVGL stats show the CPU busy at 101 to 105%. How can it be above 100%?

Read it as "the CPU is saturated". `busy` is refreshes per second times the average refresh time, divided by a fixed 1,000 ms: 7 x 145.0 ms is 1,015 ms, shown as 101%. That arithmetic matches `busy` within 1 point on all 38 saved lines. Seven refreshes of 145 ms cannot fit in 1,000 ms, so the real stats window was a little over one second. The suspected cause is that the once-a-second timer runs after a refresh that is already under way; the `t=` counter skips a second twice in the 32 MHz log. The window length was not logged. A refresh is one LVGL pass that redraws only the changed areas, about 12,000 to 25,000 pixels of the screen's 153,600, so refreshes per second are not full-screen frames per second.

### Can code run from external memory to get past the 128 kB flash limit?

No. The MSPM0G3507 data sheet lists no external memory bus or quad-SPI interface, so code must fit in its 128 kB of flash. External SPI flash or the display's SD slot can hold pictures, fonts and logs, streamed in over SPI. The LP-MSPM0G3519 has 512 kB of flash and 128 kB of RAM. `display-test/README.md`.

### The touch chip reports ID 0 instead of 0x11, and every touch reads as zero. Why?

The two touch pins are still assigned to the I2C1 block, so software I2C toggles nothing and reads zeros. Switch them to plain GPIO with pull-ups first (`DL_GPIO_initDigitalOutput`, then `DL_GPIO_initDigitalInputFeatures` with `DL_GPIO_RESISTOR_PULL_UP`, then `DL_GPIO_clearPins`), as `display-test/display_test.c` does before it reads touch. The BitNetMCU touch demo (`fox-red-labs/ports/bitnetmcu/touch/`) copied the driver but not that setup, and read ID 0 until it was added.

### Is BitNetMCU faster on the LP-MSPM0G3507 with its inner loops in SRAM?

No, keep them in flash: 2.95M against 2.64M cycles per digit at 32 MHz, and 2.96M against 2.75M at 80 MHz. The BitNetMCU author runs them from SRAM on WCH's CH32V002 to avoid flash wait states. Source: `fox-red-labs/ports/bitnetmcu/README.md`.

## Flashing and the Mac

### OpenOCD's `program ... verify` fails about every other time on the LP-MSPM0G3507. What do you do?

Run it again; the next attempt passes. Flashing BitNetMCU images on 2026-10-06, about every second `program` ended in `** Verify Failed **` and the next one gave `** Verified OK **`; `fox-red-labs/ports/bitnetmcu/mspm0/run.sh` retries up to three times. Not traced further.

### OpenOCD prints "Error: checksum mismatch - attempting binary compare" while flashing the LVGL image. Did it fail?

Not necessarily: read the next line. On the MSPM0L1306 and G3507, OpenOCD's quick checksum fails on the images flashed here and it falls back to reading the image back, which prints `** Verified OK **` when it passes (`mspm0l1306/FAQ.md`). Another page here suspects padding between sections (`portability/arduino/README.md`). The LVGL images were flashed without `verify` and ran, but the verify output was not saved for them, so padding is not confirmed.

## DriverLib, TI Drivers and Zephyr

These come from building one small app (ADC reading, serial line, LED) three ways on this board and the LP-MSPM0L1306 on 2026-10-07: [portability/three-ways/README.md](../portability/three-ways/README.md). MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, Zephyr 4.4.2. The four G3507 images were built, not run on the board.

### A TI Drivers image on the G3507 carries the DMA driver, and the app never uses DMA. Why?

TI builds the G3507 driver library with UART DMA support, so the UART driver brings the DMA driver in: 1.3 kB of code, 56 bytes of constants and 77 bytes of RAM in the no-RTOS build. The L1306's library (`drivers_mspm0l11xx_l13xx.a`) is built with `-DUART_NO_DMA` in its `.mak` file under `source/ti/drivers/lib/ticlang/m0p/` and carries no DMA driver; the G3507's `.mak` file has no such flag. Rebuilding the G3507 library without DMA was not run. Numbers from the module summary in `portability/three-ways/maps/tidrivers-g3507-nortos.map`.

### A TI Drivers program sends binary on the serial port instead of text. Why?

This was seen on the LP-MSPM0L1306, not on this board; the answer is in the [LP-MSPM0L1306 FAQ](../mspm0l1306/FAQ.md), "My TI Drivers program on the L1306 sends binary at 115200 instead of text". The suspect there is the hand-ported configuration, since TI ships no TI Drivers example for that chip. The same build on the G3507, where TI's own UART examples exist, is the check still to run; it has not been run.

### Does Zephyr have an ADC driver for the MSPM0G3507?

Not in Zephyr 4.4.2: its `drivers/adc` folder has no MSPM0 file, so call DriverLib for the ADC from your Zephyr app. Add `dl_adc12.c` and `dl_vref.c` to your `CMakeLists.txt` yourself, because Zephyr's MSPM0 Kconfig only switches on the GPIO, UART, timer and CAN parts of DriverLib. In the test app that DriverLib code is 30 of the 68 glue lines. Details in the [LP-MSPM0L1306 FAQ](../mspm0l1306/FAQ.md), "Does Zephyr have an ADC driver for the MSPM0?".

### Do you need to call `UART_init()` before `UART_open` with MSPM0 TI Drivers?

No: the MSPM0 SDK's UART driver has no `UART_init()`; it sets up its buffers inside `UART_open`. A missing `UART_init()` was suspected while chasing the L1306's unreadable output; `source/ti/drivers` of SDK 2.11.00.07 has no such function, so the suspicion was withdrawn.

### Can you drop the C library heap from a no-RTOS TI Drivers build?

No: keep a heap. The first note called the 2 kB heap unused, but `GPIO_init` (`source/ti/drivers/gpio/GPIOMSPM0.c`) creates a semaphore with `SemaphoreP_createBinary`, and the no-RTOS driver porting layer allocates it with `malloc` (`kernel/nortos/dpl/SemaphoreP_nortos.c`). How small the heap can go was not measured.

### A TI Drivers GPIO table entry without a pin-mux index sets up PA0. Is that expected?

Give every entry its pin-mux index, or mark it `GPIO_DO_NOT_CONFIG`. Read from the source, not run: `GPIO_init` takes the pin-mux index from each entry's low byte (`pincm = pinConfig & 0xFF`), and 0 is PA0's pin control register. TI's G3507 `adc_singlechannel` table has four entries without one (two ADC pins and the two UART pins). The first table copied that pattern for the LED and ADC entries; it was replaced before anything ran.

### Moving a TI Drivers build from the G3507 to the L1306: which startup file does it need?

`startup_mspm0l130x_ticlang.c` from the SDK's `source/ti/devices/msp/m0p/startup_system_files/ticlang/`, not the G3507's. The first L1306 no-RTOS build linked the G3507 startup file through a bug in the build script. Check the linker map: it names the startup file it used (`startup_mspm0l130x_ticlang.o` after the fix).

## Checks that mislead

### Does a PWM output show up on the board's USB serial port?

No. Test it with a jumper and the debugger instead. A 24 kHz square wave is not valid serial data, and the USB serial bridge shows zero bytes (it likely discards malformed bytes instead of reporting them). A jumper from PA12 to PA13 gave a valid test: with the port running, PA13 read high in 88 of 600 debugger samples, 14.7% (plus or minus 2.8% at 95% confidence), against 12.5% duty for that channel on channel 4. Channels 1 to 3 were not measured.

### The OpenOCD flash command reports a verify mismatch on a good image. Is the flash bad?

Leave `verify` off the `program` command. `program <file> verify` reported a false alarm on these images. Read the flash back another way if you need proof.

### Holding S1 does not print `S1=0`. Why?

S1 on this LaunchPad is active-high, so a held button reads 1. Check which way a button pulls before predicting what it reads. Switch outputs off before reading a pin, or two wires can look wrong.

### How big a heap does LVGL need on the LP-MSPM0G3507?

12 kB (10.3 kB usable) is enough for the touch demo. The measured heap peak on the board is 8.5 kB. A 64-bit Mac run overstates it. `lvgl-g3507/README.md`.

## Benchmark programs

### Why does the LP-MSPM0G3507 drop triggers when it runs the C2000's FOC loop at 50 kHz?

One float FOC step takes longer than a 50 kHz period on this chip, so the loop cannot run at that rate. Measured on the board with no missed period in 1,000, the highest rate is 4.26 kHz in software float, 10.94 kHz with MATHACL doing sine and cosine, and 70.80 kHz with the step rewritten in TI's IQmath fixed point on MATHACL (38.11 kHz in IQmath without MATHACL). The Cortex-M0+ has no floating-point unit, so every float operation in `foc.c` is a C library call. A dropped trigger is a PWM period that gets no new duty: the motor runs on the old value for that period. The program drops triggers that arrive while an interrupt runs and times each interrupt from a fresh trigger. If you write a motor loop for this chip, use IQmath with the MATHACL library: one step is 918 cycles against 14k in software float. TI Arm Clang 5.1.1 -O2, 80 MHz, code in flash, measured 2026-10-08. Page: `mspm0g3507/mcu-bench/ctrl/README.md`.

### Why is the LP-MSPM0G3507's software-float FOC loop slower above 7 radians?

The C library's `sinf` and `cosf` take a longer path for angles above 9π/4 (7.07 rad): about 3.8k instructions a call against about 2.3k below it, so the whole step costs about 30 percent more there. This is how TI Arm Clang's C library is written, not the chip. Keep the angle wrapped to one turn (0 to 2π) before calling them, as a real motor loop does; `foc.c` passes the raw angle because it is a fixed benchmark. A test that stops before the angle gets there reports the faster figure, so time a loop over a full sweep of angles. Counted by emulating the image's own code.

### Can the LP-MSPM0G3507 run a float PID power loop at 100 kHz?

No: use Q15 fixed point, which holds 190.9 kHz with no missed period in 1,000 and takes 41.4 percent of a 100 kHz period of CPU time. The Cortex-M0+ has no floating-point unit, so the float PID takes 1.8k cycles on average (2026-10-07), more than two 800-cycle periods at 80 MHz; it holds 39.70 kHz. Q15 reaches its duty write in 315 cycles at worst (3.9 µs, 2026-10-08 image). MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1 -O2, code in flash, measured 2026-10-08. Page: `mspm0g3507/mcu-bench/power-loop/README.md`.

### The LP-MSPM0G3507 power-loop test shows one ADC overflow flag at 100 kHz. Is the loop too slow?

No. Every one of the 1,000 duty writes landed in its period, and the rate sweep, which counts an overwritten ADC result as a missed period, passed at 190.9 kHz with no overflow. The sweep reads and clears the ADC before each trial, which fits that suspicion; no test isolated it.

### Why is the LP-MSPM0G3507's in-spec SPI speed 13.3 MHz in these tests, when the data sheet says 32 MHz?

Use the high-speed pins (PA9, PA12 to PA14, PA19) with a bus clock of 64 MHz or more if you need over 16 MHz. The data sheet SLASEX6C (Table 7.21.1 and note 1 of section 8.25) allows 32 Mbit/s only on those pins and 16 MHz on the others. The test used the display pins PB7 to PB9. With an 80 MHz bus clock the SPI clock steps are 40, 20 and 13.3 MHz, so 13.3 MHz is the fastest at or under 16: 1,488 kB/s with 16-bit frames. Page `mspm0g3507/third-party-bench/README.md`.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-09. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- TI low-power examples (sysctl_power_policy_sleep_to_standby, timx_timer_mode_pwm_x_trig_stop_restore) cut the debug link (`mspm0g3507/AGENTS.md`, `mspm0l1306/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug and replug USB (xdsdfu -e lists the probe again) | hands | proven | `mspm0g3507/AGENTS.md`, date not recorded |
| uhubctl power cycle of the probe's hub port (6 s off) when the XDS110 stops enumerating after a firmware write (the own unit's probe vanished from USB a minute after xdsdfu wrote 3.0.0.43; `uhubctl -l 1-1 -p 3 -a cycle -d 6` brought it back as MG350001) | remote | proven | `data/results/mspm0g3507.csv`, 2026-11-09 |

Restore: frl restore <backup folder> --probe MG350001 (OpenOCD program and verify, read back and compare). Regions: main flash; NONMAIN read only. (`data/results/mspm0g3507.csv`)

<!-- recovery:end -->
