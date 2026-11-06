# How do I take an LP-MSPM0L1306 LaunchPad kit to a checked result, with or without an AI assistant?

Run one local tool, `frl`, from your copy of this repository. It finds the board, checks your tools, backs the board up, builds, flashes, and checks the result through the board's own debugger. Once TI's tools are installed, the first run takes about 13 s of machine time. No AI account, no CCStudio window and no model call are needed; an assistant is optional and calls the same steps.

**Contents.** Before you start, first run, a second route, what each step does, what it promises, error table, tool versions, boards it drives, which chip is on a probe, adding a route.

Checked on 2026-10-14 on an Intel Mac (macOS 15.7) with one LP-MSPM0L1306, which ran the whole route. An Apple Silicon Mac and Linux (Ubuntu 24.04, x86_64) each built the image, and those images passed the check on the board, flashed from the Intel Mac; flashing from those two computers is untested. Windows is untested.

Terms: [docs/vocab.md](vocab.md). Here, the **probe** is the USB debugger built into the top half of the board (an XDS110); **main flash** is the chip's program memory; the **boot configuration** (NONMAIN on this chip) is the memory that sets how the chip starts, which this tool never writes; an **image** is the built program file.

## Before you start

This is the quick install: no IDE. The fuller choice with CCStudio, and what each AI app adds to your computer, side by side: [docs/install-routes.md](install-routes.md).

| What | Version tested | Size | Where |
|---|---|---|---|
| MSPM0 SDK, TI's code and examples for the chip | 2.11.00.07 | 0.4 GB download, 2.2 GB installed | [ti.com/tool/MSPM0-SDK](https://www.ti.com/tool/MSPM0-SDK), free, needs a myTI login |
| SysConfig, TI's pin and clock setup tool | 1.26.3 | 0.93 GB installed | [ti.com/tool/SYSCONFIG](https://www.ti.com/tool/SYSCONFIG) |
| TI Arm Clang, the compiler | 5.1.1.LTS | comes inside CCStudio 21.0.1 (1.8 GB installed there), or alone: the macOS Apple Silicon installer is 310 MB and the Linux x64 one 324 MB, both downloaded without a login | [ti.com/tool/ARM-CGT](https://www.ti.com/tool/ARM-CGT) |
| xPack OpenOCD, which talks to the probe | 0.12.0-7 (it reports 0.12.0+dev-02228) | about 10 MB | [GitHub releases](https://github.com/xpack-dev-tools/openocd-xpack/releases), no login. Unpack the archive for your system to `~/xpack-openocd-0.12.0-7`, or anywhere and set `FRL_OPENOCD` to it. On a Mac, if macOS blocks it as downloaded from the internet, run `xattr -dr com.apple.quarantine ~/xpack-openocd-0.12.0-7` once. Homebrew's OpenOCD 0.12.0 predates the MSPM0 target file and is refused (`FRL-E021`) |
| make and Python 3.9 or later | macOS: `xcode-select --install` | | already on most Macs |

`python3 tools/frl/frl.py doctor --board mspm0l1306` lists what is missing, with these links. The SDK sizes are measured in `mac-setup/README.md`; the SysConfig, OpenOCD and compiler sizes were measured on a Mac with `du`. Installed sizes count 1 GB as 1,024 MB; download sizes are as TI's server reports them (1 MB = 1,000,000 bytes). If you install the compiler through CCStudio instead, CCStudio 21.0.1 is a 1.9 GB download and 8.1 GB installed (also in `mac-setup/README.md`), so have about 12 GB free. Install time was not measured for this route. If your home folder name has a space in it, set `FRL_PROJECTS` to a folder without one: TI's example makefile cannot build from a path with a space (error `FRL-E070`).

## First run: about 13 s of machine time once the tools are in place

Plug the board in with a USB data cable (a charge-only cable powers it but hides it), into the connector on the top half of the board. Then, in a terminal:

```bash
git clone https://github.com/Fox-Red-Labs/fox-red-labs.git
```

```bash
cd fox-red-labs
```

```bash
python3 tools/frl/frl.py detect
```

That prints each probe's serial number. With two or more boards plugged in you must name one with `--probe`. With only one you may leave it off; that case passed this repo's tests with simulated hardware, but every run on the real bench named the probe.

```bash
./start.sh mspm0l1306 blink --probe <your serial> --pattern 150,150,150,900
```

It backs up the board, copies TI's blink example to `~/FoxRedLabs/mspm0l1306-blink-pattern/`, sets LED2 (the color LED at the bottom edge, lit red) to two short flashes and a pause, builds, flashes, and times that LED's pin (PA26) through the debugger. LED1, the separate red LED, stays dark. The board must have its jumpers as shipped: J2, J3, J12 and J13 connect the LEDs, and all nine on J101 connect the debugger. On this repo's bench the steps added up to 12.9 to 13.7 s of machine time over three runs. A pass reads like this (the level count varies):

```text
OK: Your new pattern is running on LP-MSPM0L1306. LED2 red (PA26) repeats the asked pattern 150,150,150,900 ms (timed 10 levels, tolerance 40 ms or 10%)
```

The pattern is one line in `gpio_toggle_output.c` in that folder: `static const uint32_t pattern_ms[] = {150, 150, 150, 900};`, on, off, on, off in milliseconds. Change the numbers there or with `--pattern`, and run the same command again. Each time must be 50 to 5,000 ms, at most 8 on/off pairs, and one cycle at most 6 s, so that both you and the check can see it; anything else is refused with `FRL-E070`. To answer the questions one at a time instead, run `./start.sh` with nothing after it.

The board ships with TI's out-of-box demo. The first backup you make holds it; later runs back up whatever is on the board then, so keep the first folder. To put that program back, use its folder:

```bash
python3 tools/frl/frl.py restore "<first backup folder>" --probe <your serial>
```

Restore writes main flash back and reads it back to compare; the boot configuration is never touched. It ran five times on this repo's bench, and each time the read-back matched the backup, whose sha256 (41dcaf75...) is the out-of-box image saved when the board arrived. The demo run was not watched afterwards.

## A second route: print your own message

The same tool can make the board print a line you choose over the USB cable, once a second, and check it by reading the board's serial port (115200 baud, the first of the board's two USB serial ports; J16 and J17 must be on XDS, as shipped):

```bash
./start.sh mspm0l1306 print-a-message --probe <your serial> --message "Hello from my LaunchPad"
```

It copies TI's `uart_tx_console_multibyte_repeated_fifo_dma` example and puts your text in one line of its `.c` file. A pass reads `OK: Your new message is printing from LP-MSPM0L1306. the board printed "> Hello from my LaunchPad N" 3 times on /dev/cu.usbmodem<serial>1, counting`. One run passed on the lab bench, 15 s from start to result. The check ignores the NUL byte TI's example sends after each line. The message is 1 to 40 plain characters, without quotes or backslashes. On Windows the tool looks for the COM port named "XDS110 Class Application/User UART" (written, not run on Windows); when it cannot pick one, the run ends "not verified" and you look in a terminal program instead.

## What each step does

| Command | What it does | Touches the board? |
|---|---|---|
| `frl detect` | Lists the XDS110 probes on USB. A probe is not a board | No |
| `frl identify "<printed name>"` | Matches the kit name printed on the board. An unknown name gets TI's page for the kit and an honest "no guided route" | No |
| `frl identify --board B --probe S` | Reads the chip's device ID behind that probe and checks it is the chip model of board B | Reads only |
| `frl eligible --board B --project P` | Says whether project P ran on board B, before anything is installed | No |
| `frl doctor --board B` | Checks exactly the tools this route needs, with versions | No |
| `frl backup --board B --probe S` | Reads main flash and the boot configuration twice, compares, keeps them | Reads only |
| `frl prepare --board B --pattern P` | Copies TI's example into your folder once, then sets the pattern (`--recipe print-message --message M` sets a message instead) | No |
| `frl build --board B` | Builds with the tested SDK, SysConfig and compiler | No |
| `frl flash --board B --image F --probe S` | Checks the chip ID and that the image stays in main flash, then programs and reads back | Writes main flash |
| `frl verify --board B --probe S --pattern P --image F` | Times the LED pin for 4 s and checks the flash holds that image | Reads only |
| `frl verify-image --board B --image F --probe S` | Compares the flash with an image | Reads only |
| `frl flash-prebuilt <manifest.json> --probe S` | Flashes a ready-built image after checking its manifest (chip, sha256, memory it writes), then runs the same check. The one prebuilt image is not cleared for public release | Writes main flash |
| `frl restore <folder> --probe S` | Writes a backup back to the board it came from, then reads it back | Writes main flash |
| `frl run --board B --probe S --pattern P` | All of the above in order; stops at the first failure | Yes |
| `frl status --handoff` | The last run as a short summary a person or a fresh assistant can continue from | No |
| `frl status --report` | The same summary with your home folder, user and computer name and IP addresses removed and the probe serial shortened to its last two characters, to paste into a report; nothing is sent | No |

The full command is `python3 tools/frl/frl.py <command>`. Add `--json` for the structured result. Every result has a code; [docs/frl-errors.md](frl-errors.md) explains each one, and the common ones are below.

## What it promises, and what it does not

- **The right chip model.** With two or more probes plugged in and none named, it stops before touching anything. Before any write it reads the chip's device ID and refuses a different chip model. Two LP-MSPM0L1306 boards have the same model ID, so naming the right probe is still yours to do. The order USB lists probes in never picks one.
- **What "verified" means.** The LED pin was timed through the debugger and matched what was asked, and the flash held the image just built. It does not mean anyone looked at the LED. If no check exists for a job, the result is "not verified", never a success. An assistant saying it finished changes nothing.
- **One run per probe.** On one computer, for one user, two runs of this tool cannot use one probe at once, whichever assistant or copy of the repository starts them. (Setting a different `FRL_HOME` gives a separate lock.) Other programs (CCStudio, UniFlash) do not take part in that lock; when one holds the probe, the tool reports it busy and never stops it. If a run was killed in the middle of a write, the next flash or restore prints a warning, and `frl status` gives the exact command to check the board first.
- **Backups that come back.** A backup is tied to the chip it was read from (its factory trace ID), kept outside the repository (macOS `~/Library/Application Support/FoxRedLabs/backups`, Linux `~/.local/state/fox-red-labs/backups`, Windows `%LOCALAPPDATA%\FoxRedLabs\backups`), and refused if its files changed. The boot configuration is read but never written. A backup covers the chip's own flash only.
- **Your files stay yours.** Your project is a plain folder you can open, edit and keep. Each run keeps its own record beside the backups (`runs/<run id>/`).
- **Time, split by who spends it.** The run records machine time per step. Your own hands-on time is recorded as unknown, never as zero.

## If you see this

| What you see | Do this |
|---|---|
| `FRL-E001 REFUSED: no XDS110 debug probe found on USB` | Try another USB cable: a charge-only cable lights the board but hides it. Plug into the top half of the board, straight into the computer |
| `FRL-E002 REFUSED: 8 probes on USB: ...` (any number above one) | Add `--probe <serial>`; unplug the other boards if unsure which is which |
| `FRL-E004 REFUSED: probe ... is in use by ...` | Another run of this tool has the probe: wait for it to finish |
| `FRL-E004 FAILED: probe ... could not be opened; another program (CCStudio, UniFlash or another OpenOCD) may hold it` | Close that program's debug connection |
| `FRL-E020 REFUSED: ... missing: ...` | Install what it names (table above), or point to it with the `FRL_*` variable it names |
| `FRL-E021 REFUSED: sysconfig 1.28.0 installed; this route was validated with 1.26.3` | Install the tested version, or name yours with `FRL_SYSCONFIG` and accept an unvalidated run |
| `FRL-E070 REFUSED: the project folder ... has a space in its path` | Set `FRL_PROJECTS` to a folder without spaces |
| `FRL-E006 FAILED: OpenOCD could not attach to the board on probe ...` | Check all nine J101 jumpers; if the output says "low power mode", hold S1, tap S3, keep S1 held 2 s, then run `./start.sh` again at once; unplug and replug USB afterwards |
| `FRL-E040 FAILED: LED2 red (PA26) does not follow the asked pattern; it did: ...` | Suspected: the board runs another program. The message says what the LED did. Run the same `./start.sh` command again (it rebuilds and flashes) |
| `FRL-E042 UNVERIFIED: only 18 reads a second through the probe: too slow to time a pattern` (the check needs 20) | Close other debug sessions and stop other boards' runs on this computer, then run `frl verify` again |
| `FRL-E006 FAILED: CCStudio could not read the board on probe ...: fatal: connect refused: ...` (LP-CC1354P10-1) | The chip appears to take a debugger only just after power-on (suspected, not confirmed). Unplug the probe's USB cable for 6 s, plug it back in and run the step again at once. On a hub that `uhubctl` (a command-line tool that switches power on USB hub ports) can switch, set `FRL_POWER_CYCLE=<hub>:<port>` (the location and port number `uhubctl` lists) and frl does it for you, only for `Error -242`, only when `uhubctl` lists this probe on that port, and only when both switch commands succeed; a hub that does not cut the port's power cannot reopen the window, and the error then says so |

## Tool versions and where they are found

`frl` looks in TI's default install folders (macOS `/Applications/ti` and `~/ti`, Linux `~/ti` and `/opt/ti`, Windows `C:\ti` and `%USERPROFILE%\ti`, or the folder in `FRL_TI_ROOT`) and on PATH, and uses the tested version when it finds it. When it finds only other versions, or two different copies of the tested one, it stops and lists them instead of guessing (codes `FRL-E021`, `FRL-E022`). Name a tool yourself with `FRL_MSPM0_SDK`, `FRL_SYSCONFIG`, `FRL_TICLANG`, `FRL_OPENOCD`, `FRL_XDSDFU`, `FRL_CLAUDE` or `FRL_CODEX`; a run that uses an untested version records it as unvalidated. `FRL_HOME` moves the backups, runs and locks; `FRL_PROJECTS` moves the project folders.

## With an assistant

`./start.sh mspm0l1306 blink --probe <serial> --assistant claude` (or `codex`) gives the assistant the board folder's `AGENTS.md` and the exact `frl` commands to run, then runs the check itself when the assistant finishes, including that the flash holds the image the assistant built. The check decides the result. Desktop apps can call the same steps as tools; what has been proven per app is in docs/frl-desktop-apps.md.

## Where it has run

| Computer | Result | Limit |
|---|---|---|
| Intel Mac, macOS 15.7, Python 3.9.6, LP-MSPM0L1306 | Backup, build, flash and check ran on the board on 2026-10-14, with no assistant, with Claude Code and with the Codex CLI; restore ran with a read-back match | One board, tools already installed |
| Apple Silicon Mac, macOS 26.3 | With no TI tools: `doctor` listed the four missing tools (`FRL-E020`). Installed (TI Arm Clang 5.1.1.LTS for arm64, SysConfig 1.28.1, xPack OpenOCD arm64): built in 14.7 s; that image passed the check on the board | SysConfig 1.28.1 is unvalidated (it runs under Rosetta); no board on this computer, so flashing from it is untested |
| Linux, Ubuntu 24.04 x86_64, in a container emulated on an Apple Silicon Mac, with the SDK copied in | TI's Linux installers ran unattended with no login (compiler, SysConfig 1.28.1); built in 3.9 s; that image passed the check on the board | SysConfig unvalidated; USB flashing from Linux untested; the MSPM0 SDK download needs a myTI login |
| Linux, Ubuntu 24.04 arm64, fresh container on 2026-10-23, no TI tools, no board | `./start.sh mspm0l1306 blink --dry-run` printed the plan and what a real run stops at (`FRL-E001`, `FRL-E020`) and exited 0; `doctor` listed the four tools with `~/ti` and the udev step; `detect` found 0 probes; with xPack OpenOCD unpacked to `~/xpack-openocd-0.12.0-7`, `doctor` found it | No USB in the container, so no probe access was tried |
| Windows | Not run on a Windows computer. The command is `py start.py mspm0l1306 blink`; the reader copy's CI workflow (`.github/workflows/ci.yml`) runs the dry run, `doctor`, `detect`, the results check and the tool tests on `windows-latest` | The tool looks in `C:\ti` and `%USERPROFILE%\ti`; the COM port finder is written from TI's driver names |

### Boards it drives

| Board | Result | Limit |
|---|---|---|
| LP-MSPM0L1306 | Every route above | |
| LP-MSPM0G3519 | On 2026-10-24: backup (512 kB main flash, read twice), build of TI's gpio_toggle_output in 10.0 s, write and read-back in 0.9 s, LED2 red (PB26) timed to 150, 150, 150, 900 ms; restore matched on read-back | Built with SysConfig 1.28.0 (the route validates 1.26.3). Run as separate `backup`, `flash` and `verify` steps |
| LP-MSPM0C1104 | On 2026-10-24: backup (16 kB), build in 8.9 s, write and read-back in 1.0 s, LED1 red (PA22) timed to the same pattern; restore matched on read-back | Same SysConfig limit. J7 must be on for the LED. Never mass erase this chip |
| LP-MSPM0G5187 | On 2026-10-24: backup (128 kB), build in 5.3 s, write and read-back in 0.8 s, LED2 red (PA17) timed to the same pattern; restore matched on read-back | Same SysConfig limit. J12 [2:1] on for LED2 red |
| LP-MSPM33C321A | On 2026-10-24: backup (1 MB, at the 0x10000000 alias), build in 5.0 s with the validated tools, write and read-back in 0.5 s, LED2 red (PC26) timed to the same pattern; restore matched on read-back | Needs TI's OpenOCD (below); xPack OpenOCD 0.12.0-7 has no MSPM33 driver |
| LP-AM13E230 | On 2026-10-24: backup (512 kB), build in 5.9 s (SysConfig, TI Arm Clang and the SDK's libraries; the SDK ships this example without a makefile), write and read-back in 1.3 s, LED4 red (PA5) timed to the same pattern; restore matched on read-back | Needs TI's OpenOCD. The board used here is prototype silicon (DEVICEID 0x0000002F); the route also accepts the production ID from the datasheet (PARTNUM 0x0BBE, 0x0BBE002F), not seen on a board |
| LAUNCHXL-F2800137 | On 2026-10-24: backup (256 kB main flash, read twice), build of TI's led_ex1_blinky with the pattern in 4.2 s, write and image check in 9.4 s, LED4 (GPIO20, active low) timed to 150, 150, 150, 900 ms; restore matched on read-back | OpenOCD has no C28x support, so every step runs through CCStudio's scripting (`tools/frl/dss/frl_c28.js`); CCStudio reads the pin about 42 times a second while the CPU runs, so the tolerance is two sample periods (48 ms) |
| LP-EM-CC2340R5 | On 2026-10-24: identity by `flash probe` (CC2340R52E0RKPR, 512 kB); backup of main flash and the 2 kB CCFG, each read twice; print-a-message built from TI's drivers/display in 10 s, written with DSLite in 8.8 to 9.8 s, main flash checked against the image, and the message read on the probe's serial port, three times back to back; restore of both regions matched on read-back. Asked for a different message, the check failed as it should | Every write erases the whole chip, CCFG included, because TI's tools have no smaller erase for this part (DSLite: chip erase, chip erase keeping the sectors the CCFG lists, or no erase). frl says so before writing, and the restore writes the saved CCFG back. Reading memory stops the program, so the check listens to the serial port instead of timing the LED. Needs a separate XDS110 probe and DSLite (UniFlash or CCStudio) |
| LP-CC1354P10-1 | On 2026-10-25, with an external LP-XDS110. Identity read from FCFG1, the chip's factory configuration (ICEPICK_DEVICE_ID 0x1BB7802F). Backup of the 1 MB main flash and the 2 kB CCFG (the chip's boot configuration), each read twice. The pattern project built in 17 s with the SimpleLink CC13xx CC26xx SDK 8.33.00.16. Main flash written in 7.4 to 34.6 s. The red LED (DIO6) timed three times, with three different patterns. Every restore matched on read-back. Given a wrong pattern, the check answered `FRL-E040`, as intended | This chip can refuse a second debugger connect (`Error -242`). DSLite connects twice per load, so every step here runs through CCStudio's Python scripting (`tools/frl/ccs/frl_cc13x4.py`) and connects once. The write erases and writes only the 2 kB sectors the image touches, each at its own address, and the result names them (`erases`); the board's own CCFG stays in place, unlike the LP-EM-CC2340R5. The LED pin cannot be read while the program runs, so the image logs each LED change and frl reads the log after a halt; at the halt, DIO6's own output-enable bit and output bit must agree with the log, and the check is not verified (`FRL-E042`) when they cannot be read. |
| LP-EM-CC35X1 | On 2026-10-25, with an LP-XDS110ET probe. Identity from the configuration port (DEVICEID 0x1BB9602F, CC3551E). TI's uart2echo edited to print your message, built and signed in 45 s, programmed with TI's Wi-Fi Toolbox in 23 to 24 s, and the message read on the serial port, three times with three messages. Given another message, the check answered `FRL-E040`. Every restore put the earlier program back and it printed its first line again | The chip keeps its program in external flash behind its security, and the Toolbox has no read-back. So the backup saves the fuse report, the chip's unique ID (configuration port registers 0x28 and 0x2C, read-only), the running program's first line, and a copy of a signed image you name with `FRL_RESTORE_IMAGE`, with a sha256 of every file (`FRL-E053` without one). The restore programs that copy only, on the chip with the same ID, and needs the first line back as a whole line. Before a write, frl checks that the signed vendor image holds every byte of the `.out`, then programs a hashed copy; a stale `toolbox/` folder beside a new build is refused with `FRL-E031`. The flash has two layouts, two slots for over-the-air updates or one large slot, and an image over the other layout stays silent. frl refuses that write with `FRL-E036`; the first time on a chip, say which layout is on it with `FRL_CC35_LAYOUT=ota`, `single`, or `erased` after running TI's `full_flash_erase` yourself. A failed reset after the write answers `FRL-E042`. frl never activates the chip or changes its fuses: activate it first (`cc35x1/README.md`). The board needs its own USB-C cable for power |
| LP-EM-CC2755P10 | Build of TI's drivers/empty in 18.5 s. The debugger did not connect on 2026-10-24 (`cannot read IDR`, three tries), so identity and backup have not run. The write is off | Same driver limit; a CC27xx chip erase can also close the debug port |

The LP-CC1354P10-1 project is in `tools/frl/examples/cc1354p10-blink/`, because the SDK from TI's GitHub has no examples folder. Tell frl where the SDK is: `export FRL_F2_SDK=~/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16`.

The LP-MSPM33C321A route uses TI's OpenOCD, CCSTUDIO-OPENOCD 1.5.0 from <https://www.ti.com/tool/download/CCSTUDIO-OPENOCD/1.5.0> (built from github.com/TexasInstruments/ti-openocd), because xPack OpenOCD 0.12.0-7 has no MSPM33 flash driver. Unpack it to `~/ti/ti-openocd`, or set `FRL_TI_OPENOCD` to its `openocd`. The run above used a build of that repository's `ti-release` branch at commit cb22a31e.

Repeatability, 2026-10-24: one backup, then three write and LED checks back to back, then restore, on six boards: 18 of 18 passed and every restore matched on read-back (LP-MSPM0L1306, LP-MSPM0G3519, LP-MSPM0G5187, LP-MSPM33C321A, LP-AM13E230, LAUNCHXL-F2800137; rows `blink_flash_repeat`). The LP-MSPM0C1104 could not join: the chip was in a boot loop from an earlier ported example (it was restored later that day with TI's factory reset).

To show the check can fail, each route was also given a wrong pattern on 2026-10-25. The image blinked 150, 150, 150, 900 ms, and `frl verify --pattern 400,200` answered `FRL-E040` on the LP-MSPM0L1306, LP-MSPM0G5187, LP-MSPM0C1104, LP-MSPM33C321A, LP-AM13E230, LAUNCHXL-F2800137 and LP-CC1354P10-1. On the LP-EM-CC2340R5 and LP-EM-CC35X1, the message check failed for a different message. The rows are in `data/results/<board>.csv`. On the LAUNCHXL-F2800137, check one board at a time: with five other boards writing on the same computer, CCStudio read the pin only 18 times a second (about 42 alone), and frl answered `FRL-E042` instead of guessing.

### A second write path

Each route lists a second way to write: TI's DSLite, from UniFlash or CCStudio. frl uses it when the first path's tool (OpenOCD or CCStudio scripting) is not installed, or when `FRL_WRITE_PATH=dslite` asks for it, and the result names the path in `write.path`. It uses the least erasing setting DSLite has for each family, and never one that touches NONMAIN or erases everything:

| Family | DSLite erase setting | Checked on hardware, 2026-10-25 (one blink write, LED check, restore) |
|---|---|---|
| MSPM0 | Erase MAIN memory sectors by range: the image's own range | LP-MSPM0L1306 6.0 s, LP-MSPM0C1104 6.1 s, LP-MSPM0G5187 6.0 s, LP-MSPM0G3519 5.9 s. On the L1306 a 440-byte image changed only the first 2 of 64 one-kB sectors |
| MSPM33 | The same, counted from 0x0 (the image loads at the 0x10000000 alias; a range given there fails with `Invalid sector erase addresses`) | LP-MSPM33C321A 5.7 s |
| AM13 | Necessary Bank(s) Only | LP-AM13E230 5.2 s |
| C2000 | Necessary Sectors Only | LAUNCHXL-F2800137 11.5 s; the image check is DSLite's verification during the write, because the part counts 16-bit words |

Every run above passed the LED check and the restore matched; on the byte-addressed parts the flash also matched the image on a DSLite read-back. The LED check itself still runs through the first path (OpenOCD, or CCStudio scripting on the C2000).

### When the board does not answer

Before `FRL-E006`, frl tries the connect steps that erase nothing, cheapest first, and stops at the first that works. The result's `connect` field lists each try with OpenOCD's own last lines, and `worked` names the step that got through. `frl identify --board <board> --probe <serial> --connect-step "<step>"` runs one step on its own.

| Step | Where it applies | Checked on hardware, 2026-10-24 |
|---|---|---|
| Connect under reset: hold nRST through init, release it, examine the core again, halt at once, then resume | Not MSPM0 (memory access waits for reset to end, and a held nRST blocked every connect on the LP-MSPM0C1104) | LP-AM13E230 and LP-MSPM33C321A: named the chip, and the program kept running. A `reset halt` after the held nRST timed out on both, so the step releases nRST and halts instead |
| Slower probe clock: 4 MHz, 1 MHz, 100 kHz (below the route's clock only) | Every OpenOCD route | LP-MSPM0L1306 at 1 MHz and at 100 kHz (`clock speed 100 kHz` in OpenOCD's log) |
| USB port power cycle with `uhubctl` | Only a hub port where the power cut is measured (`data/recovery.json`) | None recorded: on the bench's Genesys Logic hub a port cycle left the probe powered |

The failure also reports the probe's firmware version against 3.0.0.43. The XDS110 has no reset of its own in software: `xdsdfu -r` works only together with a firmware, serial or configuration write, and `xdsdfu -m` alone leaves the probe in its boot loader. Steps that erase (an MSPM0 factory reset, a CC23xx chip erase, a C2000 unlock) are never automatic; each board's FAQ section "When the board stops answering" names them.

A write records how OpenOCD checked it (`write.verify_path`). On MSPM0 the first write of a new image often prints `checksum mismatch - attempting binary compare`: the CRC run on the chip disagreed and a byte-by-byte compare passed. On the LP-MSPM0G3519 that write took 0.7 s on 2026-10-24 (first of three, then CRC only, 0.6 and 0.7 s); the 20.6 s first write seen earlier the same day did not recur.

If `frl identify` on an LP-MSPM0G3519 first reads DEVICEID 0x00000000, the program on it holds the chip in a low-power state. The tool then resets and halts the chip, reads the ID again and lets the program run; nothing is written.

## Which chip is on this probe

`frl identify --probe <serial>`, with no `--board`, names the chip. It reads only: SWD and each family's ID register, then a JTAG scan (with TI's cJTAG switch for the CC13xx and CC26xx), then TI's CC23xx and CC27xx driver, then TI's DSLite for a C2000. The answer names the route to use when there is one. The lookup table is `data/chip-registry/`.

| Probe | Answer | How |
|---|---|---|
| LP-MSPM0L1306, LP-MSPM0G3519, LP-MSPM0G5187, LP-MSPM0C1104 | The family and its candidate parts (MSPM0G: G1518, G1519, G3518, G3519, G3529) | SWD, DEVICEID at 0x41C40004 |
| LP-MSPM33C321A | MSPM33C321A | SWD, DEVICEID at 0x80111004 |
| LP-AM13E230 | AM13 (prototype silicon, so no part name) | SWD, DEVICEID at 0x60111004 |
| LP-EM-CC2340R5 | CC2340R52E0RKPR | TI's CC23xx driver |
| LP-EM-CC35X1 | CC3551E | SWD, configuration port DEVICEID 0x1BB9602F |
| LP-CC1354P10-1 | CC13x4/CC26x4 (six parts) | JTAG IDCODE 0x2BB7802F after the cJTAG switch |
| TMDS62LEVM | AM62L32 or AM62L31 | JTAG IDCODE 0x1BBA702F |
| LAUNCHXL-F2800137 | TMS320F2800137 | TI's DSLite, PARTIDH 0x06FF0500 (OpenOCD has no C28x support; nothing answers on SWD, and the JTAG scan sees zeros) |

Checked on 2026-10-24 on 11 XDS110 probes on one Mac. The SWD DPIDR is reported but never decides the family: 0x6BA02477 came back from the MSPM0, MSPM33, AM13 and CC23xx kits alike.

## Adding a board or a route

Routes live in `data/routes.json`: the kit name and aliases, where the name is printed, how to read the chip's ID, which memory may be written, the LED pins, and per recipe the example, the source file, the tools and tested versions, and the check. Backup regions stay in `tools/benchmark/recipes.json`; whether a project ran on a board stays in `site/data/projects.json`. A route is listed only after it has run end to end on that board. The tests are `tools/tests/test_frl.py` (fake hardware) plus a real run recorded with the route.

Questions with exact error text, one per heading: the [LP-MSPM0L1306 FAQ](../mspm0l1306/FAQ.md#the-one-command-route-startsh-and-toolsfrl).
