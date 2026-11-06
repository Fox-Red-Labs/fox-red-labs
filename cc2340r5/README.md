# How do you blink an LED on a TI LP-EM-CC2340R5 LaunchPad development kit from a Mac?

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

Clip an XDS110 debug probe onto the board, build TI's `drivers/empty` example from the SimpleLink Low Power F3 SDK 9.21.00.36 with `make`, and flash it with UniFlash 9.6.0: when nothing goes wrong, the build takes 22 seconds and the flash 7 seconds. On an Intel MacBook (macOS 15.7.9) on 2026-10-05 the whole run took about 9 minutes of machine time and 3 minutes hands-on, and most of it went to a connection error that cleared once the probe was pulled off and pressed back on (gotcha 5).

Terms: XDS110 probe, SWD, UniFlash, SysConfig and CCFG are defined in [docs/vocab.md](../docs/vocab.md). Errors and fixes: [FAQ.md](FAQ.md). In every memory figure here 1 kB = 1,024 bytes, so 36 kB is 36,864 bytes.

Status, 2026-10-06: Works. TI's SDK example on FreeRTOS blinked the red LED, confirmed by eye; Zephyr 4.4.2 blinky then blinked the green LED ([zephyr/README.md](zephyr/README.md)).

Contents: [the board](#the-board), [what is in this folder](#whats-in-this-folder), [what you need](#what-you-need-before-you-start), [ten-minute path](#ten-minute-path), [build](#one-command-build), [flash](#flash-7-seconds), [gotchas](#gotchas), [what was measured](#what-was-measured), [if you see this](#if-you-see-this).

## What is in this folder, and what is not

**In this folder:** this README, `AGENTS.md` (notes for an AI assistant, every line from a run), `FAQ.md` (every error hit, with its exact text and the fix), and the demos listed under "What's in this folder" below. Results are rows in `data/results/cc2340r5.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are in the table under "What you need before you start"), and built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution. You build them with the command below.

## The board

| Part | Fact |
|---|---|
| Kit | LP-EM-CC2340R5 LaunchPad development kit |
| Chip | CC2340R52: Bluetooth LE and 802.15.4 radio, Arm Cortex-M0+ at 48 MHz |
| Memory | 512 kB flash, 36 kB RAM. The CC2340R53 is a 64 kB version on a separate board, LP-EM-CC2340R53 |
| Debugger | None on the board and no USB port. A separate XDS110 probe (LP-XDS110ET or LP-XDS110) clips on and carries power, flashing and serial |
| LEDs | Red on DIO14 (TI's SDK blink); green (Zephyr blinky) |

Sources: this README as run on 2026-10-05 and 2026-10-06, `zephyr/README.md`, `FAQ.md`, `data/results/cc2340r5.csv`.

## What's in this folder

- [FAQ.md](FAQ.md): every error and limit met on this board, with the fix and what caused it.
- [zephyr/](zephyr/README.md): Zephyr 4.4.2 blinky with the XDS110 probe, 82 seconds from the first command to running.
- [zigbee/](zigbee/README.md): TI's Zigbee light example joined to Home Assistant, two settings changed.
- [matrix/](matrix/README.md): which radio stacks and tools work on this board, RAM left per stack, and benchmarks.
- [baremetal/](baremetal/README.md): CoreMark and interrupt latency with no operating system and TI Arm Clang.
- [zephyr-ti/](zephyr-ti/README.md): Bluetooth with TI's Zephyr fork. It builds; hello_world ran after a boot-configuration change; Bluetooth samples printed nothing.
- [thread/](thread/README.md): Thread with TI's `ot-ti`. The radio co-processor image builds, links and runs (joined a CC1354P10 leader, 2026-10-06); full Thread devices fail to link (RAM).
- [oad/](oad/README.md): over-the-air updates of the blink image.
- [ble-light/](ble-light/README.md): a Bluetooth light.
- [zigbee-alarm/](zigbee-alarm/README.md): a Zigbee smoke and CO alarm.
- `src/`: the copy of TI's blink example built below.

## What you need before you start

| Thing | Why | Version used here | Where to get it |
|---|---|---|---|
| The LP-EM-CC2340R5 board **and** a debug probe (LP-XDS110ET or LP-XDS110) | This board has no USB port or probe of its own. The probe clips on and carries power, flashing and serial. | any serial | Board: <https://www.ti.com/tool/LP-EM-CC2340R5>. Probe: <https://www.ti.com/tool/LP-XDS110ET> |
| SimpleLink Low Power F3 SDK | Examples and drivers for the chip | 9.21.00.36 LTS | <https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK>, macOS installer, 536 MB |
| TI Arm Clang compiler | Builds the code | 5.1.1 LTS | Comes inside CCStudio 21.0.1: <https://www.ti.com/tool/download/CCSTUDIO/21.0.1> |
| SysConfig | Generates the pin setup code | 1.26.3 | <https://www.ti.com/tool/download/SYSCONFIG>. CCStudio also bundles one |
| UniFlash (or CCStudio) | Writes the program to the chip | 9.6.0 | <https://www.ti.com/tool/download/UNIFLASH>, macOS installer |

On the test Mac nothing new had to be downloaded for this board, because everything came from the Mac setup guide. Install steps, times and disk space are in [mac-setup/README.md](../mac-setup/README.md).

## Ten-minute path

Measured on 2026-10-05: about 3 minutes hands-on and 9 minutes of machine time, most of it a connection problem. Step 1 and step 5 happen at the bench; steps 2 to 4 in a terminal.

1. At the bench: clip the XDS110 probe onto the LP-EM-CC2340R5 and plug the probe into the Mac. You should see the probe and two serial ports. If a flash later fails with error -615, pull the probe off and press it back on (gotcha 5).
2. In the terminal: let CCStudio write the connection file (4 seconds). Do not write it by hand (gotcha 2). Replace `<any empty folder>` with a new empty folder and `<SDK>` with `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS`; the file lands in `targetConfigs/CC2340R5.ccxml` inside the imported project.

```bash
/Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace <any empty folder> -application com.ti.ccs.apps.importProject -ccs.location <SDK>/examples/rtos/LP_EM_CC2340R5/drivers/empty/freertos/ticlang/empty_LP_EM_CC2340R5_freertos_ticlang.projectspec
```

3. In the terminal: go to the copy of TI's blink example, then run the `make` command under "One-command build". It writes `empty.out` and `empty.hex` in 22 seconds.

```bash
cd src/freertos/ticlang
```

4. In the terminal: go back to this folder (`cd ../../..`), put your probe's serial number in `CC2340R5.ccxml` (`xdsdfu -e` lists them), then flash and run the program (7 seconds):

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u src/freertos/ticlang/empty.hex
```

5. At the bench: the red LED (DIO14) is on for one second and off for one second.

## One-command build

The example is the SDK's `drivers/empty`: it toggles the red LED (DIO14) every second. A copy is in `src/`. Run this from `src/freertos/ticlang`; the build took 22 seconds.

```bash
make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
     SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
     TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

The three paths are where TI's installers put the tools on the test Mac. Change them if yours are elsewhere. Output: `empty.out` and `empty.hex`. The build does not make the connection file and does not flash.

## Flash (7 seconds)

Go back to this folder first (`cd ../../..` from the build folder), then:

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u src/freertos/ticlang/empty.hex
```

`CC2340R5.ccxml` is the connection file: XDS110 probe, CC2340R5 chip, SWD on, probe picked by serial number. Change the serial number to match your probe (`xdsdfu -e` lists them). The flags, from `dslite.sh --mode flash --help`: `-f` loads the file into flash, `-u` runs the program afterward, and `-e` prints progress messages (it is `--verbose`, not erase). UniFlash's settings file for this chip family (`CC23xx_FlashProperties.xml`) sets "Erase Settings during Program Load" to "Chip Erase" by default, and this connection file does not change it.

Do not add `-v` (verify) for now. With `-v`, UniFlash reports one mismatch in the chip's boot configuration area (CCFG) and ends with `a data verification error occurred, file load failed`, although the program was written and ran after a reset (gotcha 8).

## Gotchas

1. **The board needs a second board.** **Tool or docs:** the probe is sold separately. There is no USB port on the LP-EM-CC2340R5 itself. Without the XDS110 probe you cannot power, flash or talk to it. Order the probe with the board.
2. **Do not write the connection file by hand.** **Our mistake, and Tool or docs.** UniFlash's command line needs a `.ccxml` file and does not make one. A hand-written one named the probe and the chip, looked right, and left SWD switched off, and this chip only speaks SWD. The errors that follow (`Unable to access the DAP`, `invalid processor ID`) do not say so. Let CCStudio write the file:

   ```bash
   /Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace <any empty folder> -application com.ti.ccs.apps.importProject -ccs.location <SDK>/examples/rtos/LP_EM_CC2340R5/drivers/empty/freertos/ticlang/empty_LP_EM_CC2340R5_freertos_ticlang.projectspec
   ```

   Replace `<any empty folder>` with a new empty folder and `<SDK>` with the SDK folder (`/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS` here). Four seconds. The file lands in `targetConfigs/CC2340R5.ccxml` inside the imported project.
3. **"List operations" is not a connection test.** **Tool or docs.** `dslite.sh -p` prints the flash operations without talking to the chip. It succeeded here while every real connection failed. Use a real flash or a memory read (`--mode memory`) instead.
4. **With two probes plugged in, name the one you want.** **Tool or docs.** Two boards at once is fine. The command-line tool does not say which probe it used, so put the serial number in the connection file. CCStudio has an agent skill that does this.
5. **`The target failed to see a correctly formatted SWD header` (error -615): pull the probe off and press it back on.** **Setup or handling:** most likely the probe seating; not proven (the USB cable was replugged at the same time). Here the probe powered the board, carried the serial port and could pulse reset, so it looked connected. Every software fix failed. UniFlash's `cc23xx-unlock-debug-access` mode was tried twice before the probe was reseated and returned `Failed: Connect failed` both times. Then the USB cable was unplugged, the probe was pulled off the board and pressed back on, and the cable was plugged in again. The next flash connected on the first try. Check the seating before anything else. The program a previous owner left on the board did not stop the flash.
6. **The SDK's build settings point at folders that do not exist.** **Tool or docs.** `imports.mak` names a CCStudio 20.4 folder and a placeholder user name. Pass the three paths on the `make` line as above.
7. **Zephyr works on this board with the XDS110 probe, but not through `west flash`.** **Third-party behavior** (upstream Zephyr 4.4.2). Zephyr's flash step for this board is set up for J-Link only. Build with `west`, then flash `zephyr_crc32.hex` with UniFlash. Details in [zephyr/README.md](zephyr/README.md).
8. **Verify reports one mismatch on this chip.** **Unknown.** Every image flashed with verify on (`-v`) reported the same byte, TI SDK builds and upstream Zephyr builds alike. For the SDK blink, UniFlash then reported the whole load as failed:

   `error: Cortex_M0P: File Loader: Verification failed: Values at address 0x4E020750 do not match (expected: 0x00, actual: 0xFF) Please verify target memory and memory map.`

   `Failed: File: .../empty.hex: a data verification error occurred, file load failed.` (path shortened)

   The program area verified and the program ran each time. The mismatch is in the CCFG area: 0x4E020750 is the first byte of its user record, a 128-byte area for the application's own data that sits just after the main CCFG checksum at 0x4E02074C (layout from TI's `hw_ccfg.h`). Later flashes ran without verify, so it is not known whether every image shows it. Flash without `-v` until the cause is known.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Board in hand to blinking LED | About 3 minutes hands-on, about 9 minutes machine time | Intel MacBook, macOS 15.7.9, 2026-10-05; about 8 of the 9 minutes were a connection problem | measured | `time_to_blink` (cli) in `data/results/cc2340r5.csv` |
| SDK blink build | 22 s | `make` with the three paths, SDK 9.21.00.36 | measured | `blink_build` (cli) |
| SDK blink flash | 7 s | UniFlash 9.6.0 `dslite.sh`, no verify | measured | `blink_flash` (uniflash) |
| Zephyr blinky, first command to running | 82 s (build 47 s, flash 7 s) | Zephyr 4.4.2, XDS110 probe | measured | `time_to_blink` (zephyr), `blink_build` (zephyr) |
| CoreMark, bare metal | 111.09 per second (2.31 per MHz) | 48 MHz, TI Arm Clang -O3 | measured | `coremark`, `coremark_per_mhz` (ticlang-bare-o3) |
| Interrupt latency, pend to handler, bare metal | 28 to 29 cycles | 48 MHz | measured | `irq_latency` (bare) |
| GPIO toggle by register write, -O3 | 2.1 cycles per loop pass | 48 MHz, bare metal | measured | `gpio_toggle` (register-bare-o3) |
| Blink image size, FreeRTOS | 11.7 kB flash, 13.0 kB RAM | SDK `drivers/empty` | measured | `blink_flash_bytes`, `blink_ram_bytes` (freertos) |
| RAM left with a Zigbee end device | 6.2 kB | TI's `onoff_switch` | measured | `radio_ram_left` (zigbee-end-device) |
| RAM left with a Zigbee router | 799 bytes | TI's `onoff_light`, logging on | measured | `radio_ram_left` (zigbee-router) |
| Zigbee light toggled from Home Assistant | Median 95 ms; 20 of 20 toggles landed | First pairing; about 5 s after a re-pairing about two hours later | measured | `radio_cmd_latency`, `radio_cmd_success` (zigbee-home-assistant) |

More rows, with the recipes: [matrix/](matrix/README.md), [baremetal/](baremetal/README.md), [zigbee/](zigbee/README.md).

## If you see this

- If you see `The target failed to see a correctly formatted SWD header` (error -615), pull the probe off the board and press it back on, then flash again. It cleared on the first try after reseating the probe; the cause is most likely the seating but not proven.
- If you see `Unable to access the DAP` or `invalid processor ID`, the connection file is wrong: a hand-written one left SWD switched off. Use the `.ccxml` that CCStudio writes (step 2 above).
- If you see `error: Cortex_M0P: File Loader: Verification failed: Values at address 0x4E020750 do not match (expected: 0x00, actual: 0xFF)`, flash without `-v`. The program area verified and the program ran each time.

The rest of the errors, with causes: [FAQ.md](FAQ.md).

## Limits

Verify mismatches at 0x4E020750, the first byte of the CCFG user record. Later flashes ran without verify, so it is not known whether every image shows it.

## Where these facts come from

Checked 2026-10-05 and 2026-10-06 on an Intel MacBook, macOS 15.7.9, with SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1 LTS (from CCStudio 21.0.1), SysConfig 1.26.3, UniFlash 9.6.0 and Zephyr 4.4.2. Every number above is a row in `data/results/cc2340r5.csv`; the notes for an AI assistant are in `AGENTS.md`.
