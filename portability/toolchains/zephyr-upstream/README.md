# Upstream Zephyr samples built for five TI LaunchPad kits

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. Zephyr 4.4.2, Zephyr SDK 0.17.4 GCC 12.2, build only. Nothing was flashed or run.

## Headline

Of the samples that apply to each board, nearly all build. The one failure is the same on every board and is the workspace, not the board.

| Board | Scenarios selected | Built | Failed | Filtered | Sample folders built |
|---|---|---|---|---|---|
| lp_mspm0g3507 (LP-MSPM0G3507) | 207 | 21 | 1 | 185 | 10 of 115 |
| lp_mspm0g3519 (LP-MSPM0G3519) | 207 | 23 | 1 | 183 | 12 of 115 |
| lp_mspm0l2228 (LP-MSPM0L2228) | 207 | 20 | 1 | 186 | 9 of 115 |
| lp_em_cc2340r5 (LP-EM-CC2340R5) | 207 | 24 | 2 | 181 | 12 of 115 |
| lp_mspm0l1306 (this repo's, out of tree) | 207 | 1 | 0 | 206 | 1 of 115 |

So the build rate among samples that apply (built divided by built plus failed): G3507 21 of 22, G3519 23 of 24, L2228 20 of 21, CC2340R5 24 of 26. A "scenario" is one entry in a sample's `sample.yaml` (a sample can have several, for example the philosophers sample has 9). The scope was `samples/hello_world`, `samples/basic`, `samples/synchronization`, `samples/philosophers` and all of `samples/drivers/*`.

Twister's static filters decided what applies, so most of the 185 filtered scenarios are samples written for other boards. Build success here says the board support compiles and links with these samples. It says nothing about whether they run (the G3519 upstream blinky is known to stop in the NMI handler, a run-time problem a build cannot see).

## Boards upstream

Every TI board Zephyr 4.4.2 knows (`boards/ti/`, 17): am243x_evm, am62l_evm, cc1312r1_launchxl, cc1352p1_launchxl, cc1352p7_launchpad, cc1352r1_launchxl, cc1352r_sensortag, cc26x2r1_launchxl, cc3220sf_launchxl, cc3235sf_launchxl, lp_em_cc2340r5, lp_mspm0g3507, lp_mspm0g3519, lp_mspm0l2228, msp_exp432p401r_launchxl, sk_am62, sk_am64.

LaunchPad development kits that are upstream: lp_mspm0g3507, lp_mspm0g3519 and lp_em_cc2340r5. lp_mspm0l2228 is upstream too but not on this repo's bench; it is built here as a fourth TI board, build only. `lp_mspm0l1306` is not upstream (see below). The upstream tree has no CC2755, CC2745, MSPM33 or AM13 board.

## Flash and RAM, hello_world and blinky

Bytes used, with the chip's total in brackets (from the Zephyr memory report in each build log).

| Board | hello_world flash | hello_world RAM | blinky flash | blinky RAM |
|---|---|---|---|---|
| lp_mspm0g3507 | 9,884 (128 kB) | 3,880 (32 kB) | 11,932 | 3,976 |
| lp_mspm0g3519 | 10,004 (256 kB) | 3,960 (64 kB) | 12,052 | 4,048 |
| lp_mspm0l2228 | 9,424 (256 kB) | 3,832 (32 kB) | 11,376 | 3,888 |
| lp_em_cc2340r5 | 9,600 (512 kB) | 4,112 (36 kB) | 10,108 | 4,112 |
| lp_mspm0l1306 | 9,424 (64 kB) | 3,832 (4 kB) | filtered by twister, see below | |

The G3507 blinky figure matches the 11.7 kB flash and 3.9 kB RAM of the LP-MSPM0G3507 Zephyr blinky build in `mspm0g3507/zephyr/`.

## What failed and why

| Scenario | Boards | First error line | Cause |
|---|---|---|---|
| `sample.drivers.crypto.mbedtls` | G3507, G3519, L2228, CC2340R5 | `error: Aborting due to Kconfig warnings` | `CONFIG_MBEDTLS` could not be set to y because this workspace has no mbedtls module. It is a trimmed checkout (modules: hal cmsis, cmsis_6, rpi_pico, stm32, ti; lib gui, picolibc). Not a board problem. `west update mbedtls` would very likely fix it; not run, to save disk. |
| `sample.drivers.flash.shell` | CC2340R5 only | `region 'RAM' overflowed by 7016 bytes` | The shell sample needs more than the chip's 36 kB RAM. Twister files this as "skipped (RAM overflow)", so the table counts it as a failure. A real fit limit. |

## What is filtered, and why

Same shape on all four upstream boards (numbers are G3507; the others differ by 1 to 3).

| Reason | Count (G3507) |
|---|---|
| Sample lists other boards only (`platform_allow`), mostly `samples/basic/minimal` and vendor-specific drivers | 78 |
| Board lacks a peripheral or devicetree node the sample needs: spi 13, lora 8, watchdog 8, pwm 6 (blinky_pwm, fade_led, rgb_led, servo_motor), adc 6, i2c 6, arduino headers 6, mspi 3, counter 2, video 2, one each of crc, dac, rtc, opamp, w1, dma and others | 74 |
| The sample's own `filter:` expression is false: no newlib (3, hash_map newlib variants, because this repo uses picolibc), no display, modem, gnss, led-strip, i2s, psram or peci alias, no matching crypto or AES hardware, vendor-only devicetree compatibles | 32 |
| Wrong architecture | 1 |

Specific ones worth knowing: `samples/basic/threads` is filtered on the G3507 because its `filter:` needs the `led0` and `led1` aliases to be gpio-leds and the board does not satisfy that (the other three boards build it). `samples/basic/button` needs `sw0`, and builds on all four. Twister gives a reason for every filtered scenario; `./run.sh` regenerates the per-board lists.

Boards built (sample folders): blinky, button, hash_map, sys_heap, hello_world, philosophers, synchronization on all four; threads on G3519, L2228, CC2340R5; `drivers/can` (babbling, counter) on G3507 and G3519; `drivers/uart/echo_bot` on all four; `drivers/gpio/button_interrupt` on G3519 and CC2340R5; `drivers/crypto` and `drivers/uart/async_api` on CC2340R5.

## The out-of-tree L1306

`lp_mspm0l1306` is not in Zephyr. This repo's board files are in `mspm0l1306/zephyr/boards`. The sweep includes it, cheaply: twister needs `--board-root <dir>/boards` and `-x BOARD_ROOT=<dir>` (the first makes twister see the board, the second makes the CMake build see it). Result: 1 of 207 built (hello_world, 9.2 kB of 64 kB flash and 3.7 kB of 4 kB RAM). The other 206 are filtered, 130 of them by twister's declared `min_ram` ("Not enough RAM", for example blinky and the philosophers sample) and 76 for missing peripherals. Those RAM filters are twister's own estimate, not a build result: the repo's `mspm0l1306/zephyr/build.sh` builds blinky, synchronization and philosophers on this board separately. To build them through twister you would have to override the sample's `min_ram`.

## Method

`./run.sh` (all boards) or `./run.sh lp_mspm0g3507 ...`. For each board and each sample group (hello_world, basic, synchronization, philosophers, and each directory under `samples/drivers`) it runs

`west`'s twister (`python zephyr/scripts/twister -p <board> --build-only --force-toolchain --report-filtered -M pass -T zephyr/<group>`)

then `summarize.py` turns `twister.json` and the build logs into rows and deletes the build directory. `-M pass` drops the object files of passing builds right away, so the sweep never held more than about 150 MB of builds; the sweep waits if the disk has under 500 MB free and reruns a group whose logs show a disk-full error. `analyze.py` and `report.py` write the summaries.

Two settings you need on this Mac, both already in the repo's Zephyr notes: the compiler is SDK 0.17.4's `arm-zephyr-eabi` GCC through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, and twister is told `--force-toolchain` because the boards list `zephyr`, `gnuarmemb` and `xtools` but not `cross-compile`, so twister would otherwise filter every scenario out. Twister also needed `natsort`, `junitparser`, `pytest`, `ply`, `pyserial`, `psutil` and `tabulate` installed into the venv; the base requirements do not include them.

## Versions

Zephyr v4.4.2 (tag), Zephyr SDK 0.17.4 GCC 12.2.0 (arm-zephyr-eabi), west 1.5.0, cmake 4.4.4, macOS 15 on Intel (Darwin 24.6.0). Workspace `~/zephyrproject`.

## Files

- `run.sh`: the rerunnable sweep. `summarize.py`, `analyze.py`, `report.py`: its helpers. The helpers write per-board counts, built scenarios, failures and the filtered scenarios by category.

Build only, not run. A pass here means the code compiles and links within the chip's flash and RAM.
