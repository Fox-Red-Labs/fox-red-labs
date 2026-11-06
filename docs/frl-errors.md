# What does an frl error code mean on my LP-MSPM0L1306, and what do I do?

Every result from the Fox Red Labs setup tool (`python3 tools/frl/frl.py`, `./start.sh`, and the tools desktop apps call) carries a code such as `FRL-E002`. This page is the FAQ for the codes: search it for the code you saw. Each entry says what happened, what state the board is in, what to do, and the cause tag.

Terms: [docs/vocab.md](vocab.md). DEVICEID is the chip's model number, read from it through the probe; sha256 is a fingerprint of a file's exact bytes; a read-back is reading the flash after writing it and comparing. Cause tags: **Setup or handling** (how it was plugged in, installed or set up), **Tool or docs** (a tool's limit or a missing instruction), **Chip limit** (the chip cannot do it), **Unknown** (not known), **none** (not a fault: a limit of this tool). Exit status, for scripts: 0 done or verified, 1 failed, 2 refused before anything was written, 3 not verified. The tool itself is described in [docs/frl.md](frl.md).

## All codes at a glance

| Code | Means |
|---|---|
| [FRL-E001](#frl-e001) | no debug probe found on USB |
| [FRL-E002](#frl-e002) | more than one debug probe and none selected |
| [FRL-E003](#frl-e003) | the selected probe is not on USB |
| [FRL-E004](#frl-e004) | the probe is in use by another run |
| [FRL-E005](#frl-e005) | the board behind the probe is not the board you named |
| [FRL-E006](#frl-e006) | could not read the board through the probe |
| [FRL-E007](#frl-e007) | no connect step by that name |
| [FRL-E010](#frl-e010) | this board is not in the supported routes |
| [FRL-E011](#frl-e011) | this project does not run on this board |
| [FRL-E012](#frl-e012) | this project has not been tried on this board |
| [FRL-E020](#frl-e020) | a required tool is missing |
| [FRL-E021](#frl-e021) | only an unvalidated version of a tool is installed |
| [FRL-E022](#frl-e022) | several validated versions of a tool are installed |
| [FRL-E023](#frl-e023) | the assistant command is not installed |
| [FRL-E030](#frl-e030) | the build failed |
| [FRL-E031](#frl-e031) | the image to flash is missing or changed |
| [FRL-E032](#frl-e032) | programming failed |
| [FRL-E033](#frl-e033) | programming was interrupted; the board state is unknown |
| [FRL-E034](#frl-e034) | the write would touch memory this route may not change |
| [FRL-E035](#frl-e035) | this route does not write the board: its flash driver would erase more than main flash |
| [FRL-E036](#frl-e036) | the image needs a different flash layout from the one on the board |
| [FRL-E040](#frl-e040) | the check failed: the board did not do what was asked |
| [FRL-E041](#frl-e041) | not verified: no check exists for this result |
| [FRL-E042](#frl-e042) | not verified: the check could not read the board |
| [FRL-E050](#frl-e050) | no backup was made |
| [FRL-E051](#frl-e051) | the backup does not belong to this board |
| [FRL-E052](#frl-e052) | the backup files changed since they were saved |
| [FRL-E053](#frl-e053) | the backup names no image to put back |
| [FRL-E060](#frl-e060) | the assistant stopped with an error |
| [FRL-E061](#frl-e061) | the assistant ran past its time limit |
| [FRL-E070](#frl-e070) | bad input |
| [FRL-E099](#frl-e099) | unexpected error |

<a id="frl-e001"></a>

## FRL-E001: no debug probe found on USB. What do I do?

You see: `FRL-E001 REFUSED: no XDS110 debug probe found on USB`

No XDS110 probe is visible on USB.

The board: untouched: nothing was read from or written to it.

Do this: Use a data cable, not a charge-only one (they look the same). Plug into the USB connector on the top half of the board (the XDS110 debugger half), straight into the computer rather than a hub, and check the power LED is on. If `frl detect` lists a probe with an empty serial, the computer sees it but cannot read its serial (on Linux this may be a missing udev rule from TI's CCStudio or UniFlash installer; not tested here, no board was on Linux); name it once it shows a serial.

Cause tag: Setup or handling.

<a id="frl-e002"></a>

## FRL-E002: more than one debug probe and none selected. What do I do?

You see: `FRL-E002 REFUSED: 2 probes on USB: ML130001, MG350001`

More than one probe is plugged in and none was named.

The board: untouched: nothing was read from or written to it.

Do this: Run `python3 tools/frl/frl.py detect`, then pass `--probe <serial>`. To tell which serial is which board, unplug one board and run detect again. The tool never picks a probe by order, because the order can change between runs.

Cause tag: Setup or handling.

<a id="frl-e003"></a>

## FRL-E003: the selected probe is not on USB. What do I do?

You see: `FRL-E003 REFUSED: probe ABC123 is not on USB (found: ML130001)`

The serial you named is not on USB right now.

The board: untouched: nothing was read from or written to it.

Do this: Copy the serial exactly from `frl detect`. If the board was unplugged, plug it back in and wait two seconds.

Cause tag: Setup or handling.

<a id="frl-e004"></a>

## FRL-E004: the probe is in use by another run. What do I do?

You see: `FRL-E004 REFUSED: probe ML130001 is in use by verify (pid 81300, run ?, since 2026-10-14T13:51:32)`

Another run of this tool holds the probe, or another program (CCStudio, UniFlash, another OpenOCD) has the probe open.

The board: untouched: nothing was read from or written to it.

Do this: Let the other run finish, or close the other program's debug connection. `python3 tools/frl/lock.py status` shows which run holds it. The tool never stops another process. When a run of this tool holds it, the result is refused (exit 2); when another program does, OpenOCD fails to open it and the result is failed (exit 1).

Cause tag: Setup or handling.

<a id="frl-e005"></a>

## FRL-E005: the board behind the probe is not the board you named. What do I do?

You see: `FRL-E005 REFUSED: probe AL130001 is attached to a chip with DEVICEID 0x00000000, not the LP-MSPM0L1306 (0x0BB8202F)`

The chip behind the probe is not the chip model of the board you named. The tool read its device ID and stopped before writing.

The board: read only: nothing was written.

Do this: Read the kit name printed on the board and pick that board, or name the right probe. An external debugger moved from one board to another shows up here too.

Cause tag: Setup or handling.

<a id="frl-e006"></a>

## FRL-E006: could not read the board through the probe. What do I do?

OpenOCD opened the probe but could not reach the chip. You see `FRL-E006 FAILED: OpenOCD could not attach to the board on probe <serial>`; the OpenOCD lines in the result may include `DAP Connection Error. This could be caused by the device having gone to low power mode.`

The board: untouched: nothing was read from or written to it.

Do this: Check that all nine jumpers on J101 (the debugger-to-chip header) are on and the power LED is lit, then unplug and replug USB. If the result shows the low-power line, the program on the chip goes to deep sleep right after starting: hold S1 (PA18), press and release S3 (reset), keep holding S1 for 2 more seconds, then at once run `./start.sh` again, which flashes the blink program (it does not sleep). Unplug and replug USB afterwards. The same sequence is in [mspm0l1306/AGENTS.md](../mspm0l1306/AGENTS.md).

If the message says `this user may not open probe <serial> (OpenOCD: access denied)` and the OpenOCD lines show `LIBUSB_ERROR_ACCESS`, the computer is refusing the probe to your user, not the board: on Linux install OpenOCD's udev rule once ([docs/install-routes.md, "Linux: let your user open the probe"](install-routes.md#linux-let-your-user-open-the-probe)); on Windows install UniFlash or CCStudio once for the XDS110 USB driver (not run on Windows).

Before this code appears, frl tries the connect steps that erase nothing, in order, and stops at the first that works: connect while holding the reset pin (not on MSPM0, where memory access waits for reset to end), then a slower probe clock (4 MHz, 1 MHz, 100 kHz), then a power cycle of its USB port on a hub where the power cut is measured. The result's `connect` field lists each step with OpenOCD's own last lines; a later step that worked is named in `worked`. The result also gives the probe's firmware version in `probe_firmware`; if it is below 3.0.0.43, update it from CCStudio's `ccs_base/common/uscif/xds110` folder: `xdsdfu -I <serial> -m`, then `xdsdfu -I 00000000 -f firmware_3.0.0.43.bin -s <serial> -r`. The probe has no reset of its own in software: `xdsdfu -r` works only together with a write. A step that erases (a factory reset, an unlock) is never on this list: the board's FAQ section "When the board stops answering" names it.

Cause tag: Setup or handling.

<a id="frl-e007"></a>

## FRL-E007: no connect step by that name. What do I do?

You see: `FRL-E007 REFUSED: no connect step named 'clock 50 kHz' for this board`, from `frl identify --board <board> --probe <serial> --connect-step <step>`.

The board: untouched: nothing was read from or written to it.

Do this: Copy one of the step names the result lists, for example `--connect-step "clock 100 kHz"`. The option tests one step on its own; a normal run tries them all.

Cause tag: none.

<a id="frl-e010"></a>

## FRL-E010: this board is not in the supported routes. What do I do?

You see: `FRL-E010 REFUSED: no notes for 'LP-EM-CC2340R53' here`

There is no guided route for this board.

The board: untouched: nothing was read from or written to it.

Do this: The result names the board's folder here or TI's page for the kit. Follow that folder's README by hand; those steps ran on this repo's bench only. Nothing is flashed automatically on a board without a route.

Cause tag: none; this is a limit of this tool, not a fault.

<a id="frl-e011"></a>

## FRL-E011: this project does not run on this board. What do I do?

You see: `FRL-E011 REFUSED: matter-device does not run on LP-MSPM0L1306: No radio on this chip. ...`

This project does not run on this board, for the reason given (no radio, not enough memory, no software for it).

The board: untouched: nothing was read from or written to it.

Do this: The result lists projects that did run on this board.

Cause tag: Chip limit when the reason is no radio or not enough memory; Tool or docs when it is no software for it.

<a id="frl-e012"></a>

## FRL-E012: this project has not been tried on this board. What do I do?

You see: `FRL-E012 REFUSED: <project> has not been tried on <board>`

Nobody has tried this project on this board here.

The board: untouched: nothing was read from or written to it.

Do this: Pick one of the listed projects that ran, or, with `./start.sh`, add `--assistant claude --experimental` to try it knowing nothing here says it works and no success will be claimed.

Cause tag: none; nothing has been tried.

<a id="frl-e020"></a>

## FRL-E020: a required tool is missing. What do I do?

You see: `FRL-E020 REFUSED: 4 missing: mspm0_sdk not found; sysconfig not found; ticlang not found; openocd not found`

A tool the route needs was not found, or a folder exists without its program inside.

The board: untouched: nothing was read from or written to it.

Do this: Install it from the link in the message, or point the tool at it: `FRL_MSPM0_SDK`, `FRL_SYSCONFIG`, `FRL_TICLANG`, `FRL_OPENOCD`, `FRL_XDSDFU`, or `FRL_TI_ROOT` for a whole folder of TI tools. `frl doctor --board <id>` lists everything missing at once, with the folder this computer's OS uses (`/Applications/ti` or `~/ti` on macOS, `~/ti` on Linux, `C:\ti` on Windows; [docs/install-routes.md, "Your computer"](install-routes.md#your-computer)).

Cause tag: Setup or handling.

<a id="frl-e021"></a>

## FRL-E021: only an unvalidated version of a tool is installed. What do I do?

Only versions of a tool that this route was not tested with are installed (for example `FRL-E021 REFUSED: sysconfig 1.28.0 installed; this route was validated with 1.26.3`), or the OpenOCD found has no MSPM0 target file (`OpenOCD at <path> has no target/ti_mspm0.cfg`; Homebrew's OpenOCD 0.12.0 is one).

The board: untouched: nothing was read from or written to it.

Do this: Install the tested version from the message (for OpenOCD, the xPack release in [docs/frl.md](frl.md)), or use yours anyway by naming it with its `FRL_*` variable. The run then records the tool as unvalidated.

Cause tag: Tool or docs.

<a id="frl-e022"></a>

## FRL-E022: several validated versions of a tool are installed. What do I do?

You see: `FRL-E022 REFUSED: 2 validated copies of sysconfig: <path>; <path>`

Two or more installs of the tested version were found and they are different files.

The board: untouched: nothing was read from or written to it.

Do this: Name the one to use with its `FRL_*` variable. The tool does not choose the first folder alphabetically.

Cause tag: Setup or handling.

<a id="frl-e023"></a>

## FRL-E023: the assistant command is not installed. What do I do?

You see: `FRL-E023 REFUSED: 1 missing: the codex route needs codex, which is not installed`

You chose an assistant route, and that assistant's command is not installed.

The board: untouched: nothing was read from or written to it.

Do this: Install it (the message links the official page) and sign in once, or run the same job with `--assistant none`, which needs no AI account.

Cause tag: Setup or handling.

<a id="frl-e030"></a>

## FRL-E030: the build failed. What do I do?

You see: `FRL-E030 FAILED: the build failed: make: *** [gpio_toggle_output.obj] Error 1`

The build failed.

The board: untouched: nothing was read from or written to it.

Do this: Open the log the message names and read the first error, not the last. Look it up in the board's `AGENTS.md` and `FAQ.md` (for this board: [mspm0l1306/AGENTS.md](../mspm0l1306/AGENTS.md), [mspm0l1306/FAQ.md](../mspm0l1306/FAQ.md)).

Cause tag: Unknown.

<a id="frl-e031"></a>

## FRL-E031: the image to flash is missing or changed. What do I do?

You see: `FRL-E031 REFUSED: firmware.out is missing or is not the image the manifest names (sha256 differs)`

The image to flash is missing, or it changed after it was built (its sha256 differs).

The board: untouched: nothing was read from or written to it.

Do this: Build again with `frl build`. For a prebuilt image (`frl flash-prebuilt <manifest.json>`), the manifest's `rebuild` line builds the same program from source.

Cause tag: Setup or handling.

<a id="frl-e032"></a>

## FRL-E032: programming failed. What do I do?

You see: `FRL-E032 FAILED: programming did not verify`

Programming ran but the read-back did not match.

The board: may be partly written.

Do this: Run `frl verify-image` to see what is on the board. If it is neither the old nor the new program, put the backup back with `frl restore`.

Cause tag: Unknown.

<a id="frl-e033"></a>

## FRL-E033: programming was interrupted; the board state is unknown. What do I do?

You see: `FRL-E033 FAILED: programming started and did not finish; the board state is unknown`

Programming was interrupted (cable, power, or OpenOCD stopped answering).

The board: may be partly written: its flash may hold part of an image.

Do this: Do not flash again straight away. Reconnect, run `frl verify-image` with the same image, then flash again or `frl restore`. The run stays marked as interrupted until then.

Cause tag: Unknown.

<a id="frl-e034"></a>

## FRL-E034: the write would touch memory this route may not change. What do I do?

You see: `FRL-E034 REFUSED: the image writes 16 bytes at 0x41C00000, outside main flash`

The image would write outside the memory this route may change, for example the boot configuration.

The board: untouched: nothing was read from or written to it.

Do this: Build the image for main flash only. A first demo never writes the boot configuration or erases all of flash.

Cause tag: Tool or docs.

<a id="frl-e035"></a>

## FRL-E035: this route does not write the board: its flash driver would erase more than main flash. What do I do?

You see: `FRL-E035 REFUSED: this route does not write LP-EM-CC2340R5: TI's OpenOCD driver erases the whole chip and writes the CCFG`

The board's route can back up, build and check, and its write step is switched off because the only flash driver it has erases every main sector and rewrites the CCFG. On a CC27xx, a chip erase can also close the debug port until a full image is written over SACI.

The board: untouched: nothing was written.

Do this: Run `frl backup` and `frl build` as usual. To put the image on the board, use the board's own guide (`cc2340r5/README.md`, `cc2755p10/README.md`).

Cause tag: Tool or docs.

<a id="frl-e036"></a>

## FRL-E036: the image needs a different flash layout from the one on the board. What do I do?

You see: `FRL-E036 REFUSED: the board holds the two-slot OTA layout and this image needs the one-slot layout; programming over it leaves the board silent`, or `frl has not programmed this chip before and cannot read its flash layout back`.

On the LP-EM-CC35X1 the program sits in external flash in one of two layouts, set by the build: two slots for over-the-air updates (`"ota": true` in the build's `external_memory_configurator.json`; TI's examples and uart2echo) or one large slot (`"ota": false`). An image programmed over the other layout does not start, even TI's own, until the flash is erased. The Toolbox cannot read the flash back, so frl keeps the layout it last programmed on each chip, by the chip's unique ID.

The board: untouched: nothing was written.

Do this: If frl has not programmed this chip before and you know what runs on it, say so once: `FRL_CC35_LAYOUT=ota` (two slots) or `FRL_CC35_LAYOUT=single` (one slot) before the same command. If the layouts differ, or you do not know, erase it with TI's Toolbox yourself (it removes the program on the board): `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> full_flash_erase --flash_type AUTO_DETECT --activation_type sdk_example_key`, then run the command again with `FRL_CC35_LAYOUT=erased`. frl never erases this chip itself.

Cause tag: Chip limit.

<a id="frl-e040"></a>

## FRL-E040: the check failed: the board did not do what was asked. What do I do?

You see: `FRL-E040 FAILED: LED2 red (PA26) does not follow the asked pattern; it did: on 498, off 497, ... (ms)`

The check read the board and it is not doing what was asked: the LED timing differs, the LED pin is not an output, the flash is not the image just built, or, for the message route, `nothing arrived on <port> in the check's time` or `the serial port shows other text, not your message`.

The board: read only: nothing was written by the check.

Do this: The message shows what the LED did. Run the same `./start.sh` command again (it rebuilds and flashes), or check the pattern numbers in the source the result names. For the message route with nothing arriving, move J16 and J17 to XDS (right, as shipped) and close any terminal program holding the port. An assistant saying it finished does not change this result.

Cause tag: Unknown.

<a id="frl-e041"></a>

## FRL-E041: not verified: no check exists for this result. What do I do?

You see: `FRL-E041 UNVERIFIED: no check exists for speed-test on LP-MSPM0L1306`

No automatic check exists for this job on this board, or the LED matched but no built image was available to compare.

The board: as the job left it.

Do this: Look at the board yourself and compare with what was asked. The run is recorded as not verified.

Cause tag: Tool or docs.

<a id="frl-e042"></a>

## FRL-E042: not verified: the check could not read the board. What do I do?

You see: `FRL-E042 UNVERIFIED: only 50 reads a second through the probe: too slow to time a pattern`

The check could not read the board well enough to decide: too few reads a second, too short a check, or, for the message route, `could not find the serial port for probe <serial>` (on Windows, which this tool does not search, it adds "on this OS (untested here)").

The board: read only: nothing was written.

Do this: Close other debug sessions and run `frl verify` again, with `--seconds` longer for slow patterns. When the serial port was not found, open the board's first USB serial port at 115200 baud in a terminal program and look for your message yourself.

Cause tag: Setup or handling.

<a id="frl-e050"></a>

## FRL-E050: no backup was made. What do I do?

You see: `FRL-E050 FAILED: <the backup tool's message>`

The backup step failed before a backup existed.

The board: read only: nothing was written.

Do this: Read the message, fix the cause, and back up again before any write.

Cause tag: Unknown.

<a id="frl-e051"></a>

## FRL-E051: the backup does not belong to this board. What do I do?

You see: `FRL-E051 REFUSED: this backup was read through probe ML130001, not M3010001; it belongs to another board`

The backup belongs to another board: it was read through another probe or from another chip.

The board: untouched: nothing was read from or written to it.

Do this: Use the backup made from this board. Backups are matched to the chip's factory trace ID, so a backup cannot be put on a different board by mistake.

Cause tag: Setup or handling.

<a id="frl-e052"></a>

## FRL-E052: the backup files changed since they were saved. What do I do?

You see: `FRL-E052 REFUSED: backup.json changed since it was saved (its sha256 no longer matches index.json)`

The backup's files or its backup.json changed after it was saved.

The board: untouched: nothing was read from or written to it.

Do this: Use another backup of this board. A changed backup is never written to a board.

Cause tag: Unknown.

<a id="frl-e053"></a>

## FRL-E053: the backup names no image to put back. What do I do?

You see: `FRL-E053 REFUSED: this backup names no image to put back: the chip has no read-back, so the restore reprograms a signed image`

The LP-EM-CC35X1 keeps its program in external flash behind the chip's security, and TI's Wi-Fi Toolbox cannot read it back. So frl's backup of this board saves the chip's fuse report, its unique ID, the first line the running program prints, and a copy of a signed image you name, with a sha256 of every file in it.

The board: nothing was written.

Do this: build the image you want back with `frl build` (or use TI's own), then run the backup again with `FRL_RESTORE_IMAGE=<the built .out, or its toolbox/tool_settings.json>`. For a built `.out`, frl first checks that the signed images beside it hold its bytes. The restore programs the saved copy only, on the chip with the same unique ID, and checks that the program prints the same first line as before, as a whole line. Rebuilding or deleting the build folder later does not change the copy.

Cause tag: Tool or docs.

<a id="frl-e060"></a>

## FRL-E060: the assistant stopped with an error. What do I do?

You see: `FRL-E060 FAILED: the assistant stopped with exit 1`

The assistant stopped with an error before the board was checked.

The board: as the assistant left it.

Do this: Read `assistant.err` in the run's folder, which the error message names. You can continue by hand with the `frl` commands.

Cause tag: Unknown.

<a id="frl-e061"></a>

## FRL-E061: the assistant ran past its time limit. What do I do?

You see: `FRL-E061 FAILED: the assistant ran past 30 min and was stopped`

The assistant ran past its time limit and was stopped.

The board: unknown: the assistant may have been part way through a step.

Do this: Before anything else, run `python3 tools/frl/frl.py status --handoff` and `python3 tools/frl/frl.py verify --board <id> --probe <serial>`.

Cause tag: Unknown.

<a id="frl-e070"></a>

## FRL-E070: bad input. What do I do?

You see: `FRL-E070 REFUSED: the project folder ~/my blink has a space in its path, which TI's example makefile cannot build`

A value given to the tool was not usable: a pattern (each time must be 50 to 5,000 ms, at most 8 on/off pairs, one cycle at most 6 s), a serial, a project name, or a project folder whose path has a space (TI's example makefile cannot build there).

The board: untouched: nothing was read from or written to it.

Do this: The message says which value and what it should look like. For a path with a space, choose a folder without one with `--project`, or set `FRL_PROJECTS` (needed when your home folder name has a space).

Cause tag: Tool or docs.

<a id="frl-e099"></a>

## FRL-E099: unexpected error. What do I do?

You see: `FRL-E099 FAILED: RuntimeError: <details>`

Something failed that the tool has no specific code for.

The board: unknown.

Do this: The message starts with the program error's name (for example `NameError` or `RuntimeError`), which is a fault in this tool. Run `python3 tools/frl/frl.py status --handoff` and include its text and the exact error in a report. Remove anything private first.

Cause tag: Unknown.

Questions with exact error text, one per heading: the [LP-MSPM0L1306 FAQ](../mspm0l1306/FAQ.md#the-one-command-route-startsh-and-toolsfrl).
