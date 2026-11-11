<!-- experiment
id: E319
board: LP-EM-CC2755P10
date: 2026-11-10
status: works
reached: verified
line: `logger:` in an ESPHome YAML on the cc27xx and cc23xx platforms writes ESPHome's log to the LaunchPad kit's backchannel UART at 115200 baud, and `esphome logs` shows it. The first build printed nothing because ESPHome's log level reaches the compiler as a build flag the platform did not pass on; with the flag passed, the whole boot log, component dumps and sensor lines arrive. The CC2755P10 image grew by 4.5 kB of flash and 8 B of RAM.
stop: none
-->
# Can ESPHome's logger and `esphome logs` work on the Zigbee platforms?

**Short answer.** Yes. With `logger:` in the YAML, the cc27xx platform (LP-EM-CC2755P10) and the cc23xx platform (LP-EM-CC2340R5) print ESPHome's log on the probe's first serial port at 115200 baud (`/dev/cu.usbmodem<probe serial>1`), and `esphome logs <yaml> --device <port>` reads it with no extra step. The boot lines, every component's `dump_config`, the Zigbee signals and each sensor update arrive, for example `[I][logger:036]: Log initialized` and `[D][zigbee.sensor:020]: Set attribute endpoint: 4, present_value 19.437500`. ESPHome's stack-trace decoding says "no compatible analyzer found for target platform cc27xx", which is harmless.

Terms: [vocab](../vocab.md). The platforms are in [ports/esphome-ti](../../ports/esphome-ti/README.md); the first proof was [E192](2026-10-13-esphome-cc27xx-platform.md), which had no logger.

**Question.** Can `esphome logs` read a TI LaunchPad running the cc27xx or cc23xx platform, as it does for other ESPHome boards?

**What was expected, written down before the build.** ESPHome's own Zephyr logger needs Zephyr's UART API, so the platform would need a replacement `write_msg_` on TI's UART2 driver, as the cc35xx platform already had for the Wi-Fi board. The UART instance has to be a SysConfig entry on the backchannel pins (DIO1 and DIO2 on the CC2755P10). Expected cost: a few kB of flash. Expected trouble: the logger's schema asks the platform which UARTs it has and raises `NotImplementedError` for a platform outside ESPHome's tree.

**What was tried.**

1. Copied ESPHome's logger sources into the build with the Zephyr branch switched off, and added `shim/logger_f3.cpp` (UART2 open in `pre_setup`, `write_msg_` through a mutex because two tasks log). `periph.py` adds the UART2 instance to the Zigbee SysConfig file when the YAML has `logger:`.
2. First flash: only a debug line written straight to the UART appeared; ESPHome's own lines did not. A register read showed a counter added to `write_msg_` at 0: nothing reached it. Cause: ESPHome sets `ESPHOME_LOG_LEVEL` as a compiler flag, not in `defines.h`, so every `ESP_LOG` call was compiled out. The platform now passes ESPHome's `-D` build flags to TI Arm Clang.
3. Second flash: the full log. `esphome logs` worked on the first try.

**How far it got.** Verified on both boards.

| Step | Result | Cause tag | What fixed it |
|---|---|---|---|
| `logger:` with a default `hardware_uart` | `NotImplementedError` in ESPHome's `uart_selection` | Third-party behavior | The platform answers "no choice" for that option and gives no default |
| Log lines after the first flash | None, only a direct UART write | Third-party behavior | Pass ESPHome's build flags to the compiler |
| Log on the CC2755P10, 2026-11-10 | Boot, dump and sensor lines, 2.3 kB in the first 40 s | | |
| Log on the CC2340R5 | The same, with `tx_buffer_size: 256` | | |

**The line.** None reached. Cost on the CC2755P10 router: flash 276.4 kB to 280.9 kB with the log strings in, RAM +8 B (the logger's buffer is part of the object ESPHome already allocates). On the CC2340R5 end device with the logger, I2C, PWM and settings: 35.6 kB of 36.9 kB used at link time, and the FreeRTOS heap showed 1.4 kB free and 1.4 kB lowest ever over four boots.

**What this does not establish.** Two tasks write to one buffer, so a rare interleaved line is possible; not seen in about 100 kB of log. Baud rates other than 115200 were not tried. Only macOS.

**Ask.** Can ESPHome's logger take its build flags into account for external platforms, or list the platform's UARTs through a hook?

**Effort.** About 1 hour 15 minutes, of which 25 minutes went to the missing log level flag.

**Verdict.** Good. One new shim file and one SysConfig entry; the cost was the compile flag that hid every log call.
