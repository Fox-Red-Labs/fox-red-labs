<!-- experiment
id: E232
board: NUCLEO-U083RC, LP-MSPM0G3519, LP-MSPM0G5187, LP-MSPM0L1306, LP-MSPM0C1104, LP-MSPM33C321A, LP-AM13E230, ESP32-C6-DevKitC-1
date: 2026-10-20
status: works
reached: verified
line: MaJerle's STM32 UART DMA receive and transmit example (1,838 stars) runs on six TI boards from fresh AI agents, and five pass all six host checks at 115200 baud on the same method as the NUCLEO-U083RC original (632 to 636 B of RAM on the MSPM0 parts, 3.1 kB of flash at most); no TI UART has the idle-line event the example is built on
stop: no-software
-->
# Does a popular STM32 UART DMA example carry over to TI boards (LP-MSPM0G3519 and five more), and where does it behave the same as on the NUCLEO-U083RC?

The program carried over on six TI boards with a different mechanism for the idle-line interrupt on every one, and it holds at 115200 baud on five. The port is in [`ports/stm32-usart-dma-rx-tx/`](../../ports/stm32-usart-dma-rx-tx/README.md).

Terms: [vocab](../vocab.md). Contents: what was tried, results at 115200 and 921600 baud, tool failures with fixes, compatibility by area. 1 kB = 1,024 bytes. Run 2026-10-19 and 2026-10-20 on an Intel Mac with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, and for the reference STM32CubeCLT 1.22.0 (GCC 14.3) with STM32CubeU0 1.3.0.

**Question.**

Does MaJerle's `stm32-usart-uart-dma-rx-tx` (the `usart_rx_idle_line_irq_ringbuff_tx` example: UART receive into a DMA buffer, idle-line interrupt, two lwrb ring buffers, DMA transmit, a 5-byte packet parser) run on TI MCU boards, with the same observable behavior as on the NUCLEO-U083RC, when a fresh AI assistant does the port from this repo's documents?

### Why this project

It needs nothing but the board's own USB serial port. It is a public MIT project that is widely copied (1,838 GitHub stars on 2026-10-19, last push 2026-09-12), it is not an ST example and not part of this repo, and it exercises areas that differ between vendors: UART, DMA, interrupts, timing. The example was written for the NUCLEO-G071RB; the reference run moves it to the NUCLEO-U083RC by changing names and numbers (`ports/stm32-usart-dma-rx-tx/SOURCE.md`).

### Expectation

Not written down before the first runs. What the source needs, read against the chip documents, and what happened:

| Area | What the source needs | What the TI parts offer | What happened |
|---|---|---|---|
| UART | 115200 baud, 8N1 | every board has it | carried |
| DMA | RX in circular mode with half and full transfer interrupts; TX in normal mode | MSPM0 DMA has no circular mode and one done interrupt; the C1104 has one DMA channel with no UART trigger | carried with block re-arm; the C1104 has no UART DMA, so it is gap |
| Idle-line interrupt | UART event when the line is quiet | none usable (below) | replaced on every board |
| Clock | 48 MHz PLL | all have a PLL | ports left the SysConfig default (32 MHz); not carried |
| Memory | 2.4 kB RAM | 1 kB on the C1104, 4 kB on the L1306 | fits on both |
| Timing | no loss at the line rate | depends on the replacement | holds at 115200; fails at 921600 on three ports |

**Vendor position.**

The SDK header for the UART receive timeout says: "When an additional character has not been received within the set timeout, a RX interrupt will still trigger even if the FIFO level has not been reached" (`dl_uart.h`, `DL_UART_setRXInterruptTimeout`, MSPM0 SDK 2.11.00.07). It is a timeout on a non-empty FIFO. With DMA draining the FIFO on every byte, the timeout never fires. The same header says of the DMA receive trigger: "Only one interrupt source should be enabled at a time." SysConfig for the MSPM0C1104 says "UART does not support DMA for MSPM0Cxx" (read by the agent that ported to it; the DMA trigger list there has only software, two generic subscribers and ADC). STM32's UART has an idle-line flag. For the TI side no TI document was found that names an equivalent.

**What was tried.**

Fresh Claude Code agents (Sonnet 5.5, clean start) were told to port the example, keep its structure as far as the chip allows, say what the chip cannot do, and make the baud one build-time define. Each agent read the repo's documents and tested its own port; the image was then rebuilt from the repo copy where a rebuild was possible, flashed under the probe lock and run through the host check (`ports/stm32-usart-dma-rx-tx/tools/check.py`) three times, at 115200 and at 921600 baud. The host check is the same on every board and reads only the console.

Runs: one per target board; four on the LP-MSPM0G3519 (three with this repo's documents, one told to use TI's `ti-ccstudio-migrate-to-ti` skill); one on each of the others. An ESP32-C6 run stopped at the start (below). Two earlier sets were dropped: a first set ran into the account's usage limit before porting and its checks read old images; one L1306 run stopped because its probe was unplugged.

**How far it got.**

Host check results, 115200 baud, median of three runs per row (all six checks: banner, packets, wrong stop byte, 200 split packets, 500 back-to-back packets, 100 isolated packets):

| Board | Flash B | RAM B | Checks passed | Burst packets/s (500 sent, lost) | Isolated round trip ms | Idle-line replacement |
|---|---|---|---|---|---|---|
| NUCLEO-U083RC (original) | 4,824 | 2,360 | 5 of 5 run (banner 3 of 3 apart) | 2,259 (0) | 1.72 | idle-line interrupt |
| LP-MSPM0G3519 | 3,056 | 636 | 6 of 6 | 2,246 (0) | 2.86 | 1 ms timer poll |
| LP-MSPM0G5187 | 2,584 | 632 | 6 of 6 | 2,284 (0) | 1.85 | 1 ms SysTick poll |
| LP-MSPM0L1306 | 3,016 | 636 | 6 of 6 | 2,249 (0) | 2.85 | 1 ms timer poll |
| LP-MSPM0C1104 | 2,264 | 632 | 6 of 6 | 2,294 (0) | 1.53 | UART receive timeout, no DMA |
| LP-MSPM33C321A | 3,008 | 632 | 6 of 6 | 2,284 (0) | 1.80 | 1 ms SysTick poll |
| LP-AM13E230 | 9,792 | 9,501 | 5 of 6 | 2,274 (0) | 1.41 (88 of 100 answered) | UART receive timeout |
| ESP32-C6 | not run | not run | not run | not run | not run | ESP-IDF not installed, disk |

The line limit is 2,304 packets a second at 115200 baud; every board is within 3% of it. RAM on the MSPM0 and MSPM33 rows includes a 256 B stack; the AM13 figure includes the SDK's RAM sections. Compilers differ (GCC 14.3 -Os for the original, TI Arm Clang 5.1.1 -O2 for the others), so flash and RAM compare in kind, not as a ranking.

At 921600 baud (line limit 18,432 packets a second):

| Board | Burst packets/s (lost of 500) | Isolated round trip ms | Result |
|---|---|---|---|
| NUCLEO-U083RC | 15,253 (0) | 0.92 | 5 of 5 run |
| LP-MSPM0G3519 | 9,143 (0) | 28.7 | 6 of 6 |
| LP-MSPM0G5187 | 9,013 (0) | 28.5 | 6 of 6 |
| LP-MSPM0L1306 | 9,131 (0) | 28.5 | 6 of 6 |
| LP-MSPM0C1104 | 979 (447 to 452 lost) | 28.9 | burst fails 3 of 3 |
| LP-MSPM33C321A | 6,285 (152 to 181 lost) | 28.6 | burst fails 3 of 3 |
| LP-AM13E230 | 9,195 (0) | 28.4 (88 of 100 answered) | 5 of 6, 2 runs (the third had no banner and no replies: a reset fault of the check) |
| LP-MSPM0G3519, port written under TI's migration skill | 5,938 (152 to 238 lost) | 28.1 | burst fails 3 of 3 |

The isolated round trip is the host's view through the board's USB serial bridge: about 1 ms through ST-LINK and about 28 ms through the XDS110 at 921600 baud. The chip is not shown to be the cause of the difference, and the throughput on the MSPM0 rows at 921600 (about 9,100 a second against 15,253 on the ST) is bridge-limited in the same way; those rows say the chip lost nothing, not that it is slower.

Spread on the LP-MSPM0G3519 at 115200 baud, three fresh runs with this repo's documents: 2,246, 2,279 and 2,291 packets a second, 2.6 to 3.1 kB of flash, all 632 to 636 B of RAM, 6 of 6 checks for each.

Agent time and turns:

| Port | Wall time | Turns |
|---|---|---|
| G3519, three runs with documents | 8.8, 10.2, 12.7 min | 39, 38, 50 |
| G3519, TI migration skill | 12.9 min | 55 |
| G5187 | 15.9 min | 57 |
| L1306 | 7.3 min | 34 |
| C1104 | 8.4 min | 34 |
| MSPM33C321A | 8.6 min | 33 |
| AM13E230 | 13.5 min | 46 |
| ESP32-C6 (stopped before porting) | 3.5 min | 14 |

**The line.**

The example is built on the STM32 UART's idle-line flag. No TI UART tested has an equivalent that works with DMA draining the FIFO. Each agent hit it, and all six chose a replacement:

- **A 1 ms timer poll** (G3519, G5187, L1306, M33C321A): the same check as the idle interrupt runs from a timer tick, so a packet is answered up to 1 ms late. The upstream code's own comments describe this for chips without the interrupt.
- **The UART receive timeout** (C1104, AM13E230). On the C1104, which has no UART DMA, the interrupt drains the 4-byte FIFO into the same 64-byte buffer; it loses 447 to 452 of 500 packets at 921600 baud. On the AM13E230 the timeout also triggers the DMA; 12 of 100 isolated packets were answered only when the next packet arrived, in every run (three on this board plus the assistant's own tests), and the agent could not find the cause in its 25 minutes. Changing the timeout from 4 to 15 and adding a software flush did not change it.
- **The other mismatches**: MSPM0 DMA has no circular mode and no half-transfer interrupt (RX re-arms a 64-byte block in the done interrupt); the G5187 has one DMA interrupt for both channels, read with a pending-interrupt call; the MSPM33C321A has block-done only.

The ports left the clock at the SysConfig default (32 MHz) where the original runs at 48 MHz, so timing across boards compares different CPU clocks; the console is the limit at these baud rates, not the CPU.

### What a person still did by hand, and where a tool failed

| Step or failure | Cause tag | Fix or workaround |
|---|---|---|
| Backing up the G3519's 512 kB main flash: DSLite `memory` stopped with "Memory map prevented reading 0x00080000" | Tool or docs | read with OpenOCD; DSLite's `-r` takes 32-bit words with `-s 32` and wants pieces on the AM13 and MSPM33 |
| Restoring a G-series board: DSLite writing a binary at the data-flash address erased main flash; the core then hid behind the debug port (reset loop) | Tool or docs | TI's wait-for-debug hold (the sequence on the [C1104 lockout page](2026-10-19-mspm0c1104-lockout-root-cause.md), written for the C1104 and worked unchanged on the G3519 and G5187), then OpenOCD master `flash write_image`; read back identical |
| A DSLite reset-run hung 7 hours and held the probe lock | Tool or docs | wrap it in `perl -e 'alarm 90; exec @ARGV'` |
| OpenOCD ending a connection leaves a second reset, so the banner restarts mid-line and the check read garbled text | Setup or handling | reset with `xds110reset -a toggle` instead; first pulse after an OpenOCD write gave no output on two boards (2 of 3 banners read) |
| The AM13E230 does not start a new program after a DSLite flash unless the flash uses `-r 1`; DSLite then ends with "Target is not halted" after a good load | Tool or docs | accept the message after "Verified"; start with a DSLite system reset |
| SysConfig 1.28.0 turns UART internal loopback on for a new instance on the G5187 | Tool or docs | set it off in the `.syscfg` |
| The G5187 and G3519 SDK ships SysConfig 1.28.x where the board notes validate 1.26.3 | Setup or handling | built with 1.28.0 |
| The MSPM33C321A refused every debug connection from 2026-10-19 23:39 to 2026-10-20 09:21 ("Connection to MSPM33 core failed ... Debug access within NONMAIN was disabled or enabled with password"); `xds110reset` did not help; it answered again after the board was reseated | Unknown (a stacked BoosterPack is suspected) | none by us; no force, no erase |
| The LP-MSPM0L1306 left USB after a replug, then returned after reseating | Setup or handling | none |
| Each agent cloned the 520 MB upstream repository (drivers for every STM32 family) and the disk filled to under 1 GB | Setup or handling | one read-only copy of the example and lwrb (192 kB) given to later agents; one agent deleted ST's driver folder in its own clone to continue |
| ESP-IDF 6.1 is not installed (about 3.2 GB) | Setup or handling | the ESP32-C6 port was not run |
| Two runs shared the G3519 probe: both agents' own checks read whichever image was last flashed | Setup or handling | each final image was flashed outside the agents' own runs |
| TI's migration skill is built to dispatch helper tasks and use CCStudio's MCP servers; CCStudio was not open and the agent wrote the plan and report itself, to stay in its 25 minutes | Setup or handling | not a test of the skill's own pipeline |
| The migration skill (`ti-ccstudio-migrate-to-ti`) and the repo's porting layers (`portability/everywhere/`, the shims, `mspm0g3507/stm32-port-eval/`) cover STM32 HAL examples and TI-to-TI moves. This project uses STM32 LL. Nothing in them maps LL calls, DMA channels or the idle-line flag | Tool or docs | each agent rewrote `usart_init` and the interrupt handlers from the SDK's DriverLib by hand |
| The L1306 agent found this repo's G3519 port of the same project and adapted it | Setup or handling | recorded as an adaptation, not an independent port |

### Compatibility by area

Reference: the NUCLEO-U083RC run of the same example (same checks, same method). Levels as in `docs/ports.md`. The whole record per board is in `site/data/ports.json` once merged.

| Area | G3519 | G5187 | L1306 | C1104 | M33C321A | AM13E230 |
|---|---|---|---|---|---|---|
| build | builds | builds | builds | builds | builds | builds |
| memory | measured | measured | measured | measured | measured | measured |
| uart | measured | measured | measured | measured | measured | runs |
| dma | same | same | same | gap (no UART DMA) | same | runs |
| interrupts | gap (no idle-line) | gap | gap | gap | gap | gap |
| clocks | runs (32 MHz, not 48) | runs | runs | runs | runs | runs |
| timers_pwm | runs (1 ms tick) | runs | runs | na | runs | na |
| timing | measured | measured | measured | gap at 921600 | gap at 921600 | measured (12% isolated late) |

ESP32-C6: every area is not run; the reason is ESP-IDF not installed.

- Chain a second timer capture on the RX pin edge to find line idle for real (the receive timeout needs bytes in the FIFO).
- A DMA trigger from the UART receive timeout with a FIFO threshold of one, which two agents tried and dropped on the G3519 (answers some packets, strands bytes for others).
- Run the two other G3519 runs and the skill run at 921600 baud.
- Port to the ESP32-C6 once ESP-IDF is installed, and to the LP-MSPM0G3507, which is not in this set.
- Use a logic analyzer on the TX pin to remove the XDS110 bridge from the timing numbers.

**Ask.**

The unexplained 12% on the AM13E230 (isolated packets answered only with the next), and an idle-line equivalent on the MSPM0 UART that survives DMA draining the FIFO.
