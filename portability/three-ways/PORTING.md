# Port the three-ways app.c to a new board: five calls, a build, a serial check

You write five small functions for your board and the same 81-line `common/app.c` runs on it. On 11 boards that took 35 to 66 lines of glue and 5 to 41 lines of configuration, and a family you have already ported takes minutes.

Terms: [docs/vocab.md](../../docs/vocab.md). **Glue** is the code that connects `app.c` to one board's drivers. **Configuration** is the pin and clock setup (SysConfig text, C tables or a header). Counts are code lines: no blank lines, no comment-only lines (`count.py`). 1 kB = 1,024 bytes. Measured 2026-10-25 with TI Arm Clang 5.1.1.LTS, C2000 compiler 25.11.1.LTS and GCC 14.2.0 for Linux. Numbers come from `data/results/<board>.csv` (test ids `app_glue_lines`, `app_config_lines`, `port_flash_bytes`, `port_ram_bytes`, `port_example_behavior`).

## The 11 boards

Every row passed `serial_check.py` on the board. Overview and the MSPM0 TI Drivers and Zephyr comparison: [README.md](README.md).

| Board | Layer | Glue lines | Config lines | Flash, bytes | RAM, bytes |
|---|---|--:|--:|--:|--:|
| LP-MSPM0L1306 | DriverLib | 39 | 34 | 2,320 | 272 |
| LP-MSPM0G5187 | DriverLib | 39 | 33 | 2,456 | 272 |
| LP-MSPM0G3519 | DriverLib | 39 | 34 | 2,368 | 272 |
| LP-MSPM0C1104 | DriverLib | 39 | 30 | 2,360 | 184 of 1,024 (measured) |
| LP-MSPM33C321A | DriverLib, HSADC glue | 37 | 41 | 2,816 | 272 |
| LP-AM13E230 | DriverLib, AM13 glue | 38 | 35 | 9,376 | 9,205 |
| LP-EM-CC35X1 | TI Drivers + FreeRTOS | 48 | 14 | 21,236 | 44,392 |
| LP-EM-CC2755P10 | TI Drivers + FreeRTOS | 48 | 14 | 20,179 | 38,720 |
| LP-EM-CC2340R5 | TI Drivers + FreeRTOS | 48 | 14 | 18,801 | 13,914 |
| LAUNCHXL-F2800137 | C2000Ware driverlib | 35 | 29 | 5,064 (RAM image) | 1,060 |
| TMDS62LEVM (AM62L) | Linux sysfs | 66 | 5 | 67,680 (stripped binary) | not measured |

The L1306, G5187, G3519 and C1104 share one glue file, so a new MSPM0 board costs 0 glue lines.

## The glue contract

`common/app.h` declares five calls. Your glue file defines them and a `main` (or a thread) that calls `board_init()` once, then `app_run()`.

| Call | What it must do |
|---|---|
| `void board_init(void)` | Clocks, pins, ADC, UART at 115200 8N1 and the LED. Runs once, before anything else. |
| `uint16_t adc_read(void)` | Block until one conversion finishes. Return the raw 12-bit count, 0 to 4095. |
| `void uart_write(const char *s, uint32_t len)` | Block until all `len` bytes are sent on the backchannel UART. Send raw bytes: no added `\r`, no buffering that drops text. |
| `void led_toggle(void)` | Flip the user LED once per call. |
| `void sleep_ms(uint32_t ms)` | Wait `ms` milliseconds. It must work for 1000. |

Timing rules the checker enforces: `app_run()` never returns, prints one line `t=<n> adc=<raw> avg=<avg>` about once a second, and `serial_check.py` fails any gap outside 0.9 to 1.2 s. `app.c` has no `printf` and includes only `app.h`. Do not edit `app.c` or `app.h`.

## Pick your starting point

| Your target | Start from | Change |
|---|---|---|
| Another MSPM0 with an ADC12 (G, L, C parts) | `driverlib/glue_driverlib.c` + `driverlib/g3519.syscfg` | Nothing in glue. In the `.syscfg`: device, package, board, UART pins, LED pin, ADC channel |
| MSPM33 (HSADC, no ADC12) | `driverlib/glue_driverlib_hsadc.c` + `m33c321a.syscfg` | Only `adc_read` differs from the MSPM0 file |
| AM13 | `driverlib/glue_am13.c` + `am13e230.syscfg` | Own ADC sequencer calls and `Device_Init` |
| CC23xx, CC27xx, CC35xx (TI Drivers + FreeRTOS) | `tidrivers/cc2755p10/glue_tidrivers_cc2755p10.c` + `app.syscfg` | CC2340R5 reuses the file and changes the `--board` line of the 14-line `app.syscfg` |
| C2000 | `c2000/glue_c2000.c` + `c28x_preinclude.h` | ADC channel, SCI pins, LED pin |
| Linux with sysfs ADC, UART and LED | `linux/glue_linux.c` + `config_am62l.h` | The three paths in the header |
| MSPM0 on TI Drivers | `tidrivers/glue_tidrivers.c` | No SysConfig module: 130 to 136 lines of hand-written C tables |
| Anything else | The closest layer above | Write the five calls from that layer's driver example |

## A new board in a family you already ported

Example: a new MSPM0 LaunchPad, id `<id>`. Run from the repo root.

1. **Copy the configuration.** `cp portability/three-ways/driverlib/g3519.syscfg portability/three-ways/driverlib/<id>.syscfg`. Edit device, package, board, `UART_0` pins, the LED pin and the ADC channel. The glue file stays as it is.
2. **Add a build target.** In `driverlib/build.sh` add the startup file for the family to the `case` (for example `<id>) STARTUP=startup_mspm0g351x_ticlang.c ;;`) and the id to the usage line. Other layers have their own script: `build_c1104.sh`, `build_m33.sh`, `build_am13.sh`, `tidrivers/build_cc2340r5.sh`, `build_cc2755p10.sh`, `build_cc35x1.sh`, `c2000/build.sh`.
3. **Build.** `portability/three-ways/driverlib/build.sh <id>`. It prints the sizes and the linker map's memory table. SysConfig 1.28.0 or newer is needed for the C1104.
4. **Preflight.** `python3 tools/preflight_image.py <id> portability/three-ways/driverlib/build-<id>/three_ways.out`. Exit 0 is safe, 1 is a warning to read, 2 is a refusal. Never flash over a refusal (see gotcha 5).
5. **Back up.** Read main flash and the boot configuration area with DSLite (`dslite.sh --mode memory -c <ccxml> -a <address> -l <length> -o <file>`) twice and compare the two files. Boards with a Back up row in their `AGENTS.md` list the addresses.
6. **Flash.** `dslite.sh --mode flash -c <ccxml> -e -f -v <image.out>`, with your probe's serial in the `.ccxml`. Press nRST after the flash.
7. **Check the serial line.** `python3 portability/three-ways/serial_check.py <port> 12 /tmp/run-driverlib-<id>.txt`. The port is the probe's second one, usually ending in `1`. It prints `PASS` or what failed.
8. **Restore.** Write the backup files back with DSLite, then read the flash back and compare.

## A new family

You write one glue file and one configuration. What that cost, measured:

| Family | New glue | New config | Hands-on |
|---|--:|--:|---|
| MSPM33 (HSADC) | 37 lines, only `adc_read` is new | 41 | not timed |
| AM13 | 38 lines | 35 | not timed |
| CC35X1 (TI Drivers) | 48 lines | 14 | about 25 min, one fix (gotcha 1) |
| CC2755P10 | 0 (copy of the CC35X1 file, one comment changed) | 14 | about 20 min |
| CC2340R5 | 0 (same file) | 14, one line differs | about 12 min |
| C2000 | 35 lines | 29 (24 of setup, 5 in the preinclude) | about 12 min |
| Linux (AM62L) | 66 lines | 5 | not timed |

For a TI Drivers family, copy the SDK's `drivers/empty` example, swap in the glue and `app.syscfg`, and add `app.c` to its makefile; `build_cc2755p10.sh` does this and is the template.

## Gotchas

**1. If every line prints at once (7,573 lines in 2 s) on TI Drivers with FreeRTOS, split the sleep.** POSIX `usleep()` fails for 1 s or more, so `sleep_ms(1000)` returns at once. Call `sleep(ms / 1000u)` and then `usleep((ms % 1000u) * 1000u)`, as the CC2755P10 glue does. Source: `data/results/cc35x1.csv`, `port_example_behavior`.

**2. If each line ends in `\r\r\n` and the checker reports 2 s gaps on Linux, clear `OPOST`.** A Linux tty turns `\n` into `\r\n`. The glue clears `OPOST`; run `stty -F /dev/ttyS0 opost` afterwards to give the console its setting back. Source: `linux/README.md`.

**3. If the C28x build stops on `uint8_t`, preinclude `c28x_preinclude.h` for `app.c` only.** The C28x `stdint.h` has no 8-bit type, and `app.h` uses `uint8_t`. The 5-line preinclude adds it. If you also preinclude it into a file that includes driverlib you get `invalid redeclaration of type name "uint8_t"`, because C2000Ware defines it. Source: `c2000/README.md`, `c2000-launchpad/AGENTS.md`.

**4. If the ADC pin is not wired, read an internal channel.** A floating pin gives readings that drift or swing. On the G3519, PA25 swung between 900 and 2,600; the internal temperature sensor (channel 11, `ADC121.disChan11 = true`, 2.5 V reference) read 1,085 to 1,086. On the CC2755P10 and CC2340R5, `adc0.internalSignal = "Battery Channel"` reads VDDS/3: 1,374 to 1,375 and 1,368 to 1,369 counts at about 3.3 V. Source: `data/results/mspm0g3519.csv`, `cc2755p10.csv`, `cc2340r5.csv`.

**5. If `preflight_image.py` refuses with "no memory map", add the map; do not flash.** The check needs the chip's flash and RAM regions in `data/recovery.json` under `boards.<id>.memory`: `regions` as `[name, base address, size]`, `config` for areas like CCFG, and `source` naming the linker map. Take the numbers from your build's `.map` file, run preflight again, and expect a warning only for the intended boot-configuration write. Source: `data/results/cc2755p10.csv`, `cc2340r5.csv`, `cc35x1.csv`.

**6. If the board has 1 kB of RAM (LP-MSPM0C1104), trust a stack measurement, not the linker.** The app fits: the map shows 80 bytes (16 bss plus the 64-byte stack the linker file reserves), and painting RAM with `0xAA` and running 5 s showed 184 bytes used, 840 free. CoreMark, Dhrystone and a 256-point FFT did not fit. Source: `data/results/mspm0c1104.csv`.

**7. If the MSPM33 build stops on `ADC12_0_INST` or `DL_ADC12_*`, use the HSADC glue.** The MSPM33 has the HSADC block and no ADC12. Use `glue_driverlib_hsadc.c`: it triggers sequencer 1 and reads `HSADC0_RESULT_INST`. I2C and interrupt-group names differ too. Source: `mspm33c321a/FAQ.md`.

**8. If the AM13E230 ADC reads 0 on every sample, move the input to PA17.** PA16 (ADCIN15, the pin TI's ADC example uses) read 0 with nothing connected; PA17 (ADCIN12) on the same image read 157 to 209. The PA16 cause is not confirmed. Source: `data/results/am13e230.csv`.

**9. If the checker shows a line from the old image or a reset mid-capture, pulse reset after flashing and open the capture after it.** Seen on the G3519 after a DSLite flash. Source: `data/results/mspm0g3519.csv`.

**10. If TI Drivers with FreeRTOS fails to link with `.bss` size `0x1458` against `0x1000` (LP-MSPM0L1306), use DriverLib.** TI's FreeRTOS settings for that board ask for a 3,072-byte heap on 4 kB of RAM. Source: `README.md`, problem 7.

**11. If serial stays silent after programming over the CC35X1 Wi-Fi CSI program, run `full_flash_erase` first.** It takes about 16 s; then program as usual. Source: `cc35x1/FAQ.md`.

**12. If `git diff --stat` shows every row of a results file changed, your script wrote CRLF line endings.** The results files use LF. Append rows as text or write with `lineterminator="\n"`, then check the diff size before you commit.

## Record the new board

So the table above grows, add these the day you run it.

1. **Results rows.** Append to `data/results/<board id>.csv` (LF endings), one row each for `app_glue_lines`, `app_config_lines`, `port_flash_bytes`, `port_ram_bytes` and `port_example_behavior`, with the shared variant name for your layer (`driverlib`, `three-ways-app-tidrivers`) and `status` `measured`. Put the map in `source`, `linker` or `serial` in `checked`, and say in the note what was not observed (for example, "LED toggle by code, not observed"). Count lines with the `count.py` rule.
2. **Matrix row.** In `portability/matrix.csv`, the row "Plain C app, per-layer glue": add the board to the `to` list, raise `sample`, `unchanged` and `with_glue` by one, and add its glue, config, flash and RAM to the evidence text.
3. **Check.** `python3 tools/results/check.py` must print `OK`. Then rebuild the site data with `python3 tools/site_build.py` and run `python3 tools/site_check.py`.
4. **Save the map** as `maps/<layer>-<id>.map`, so every number has a source.
