# Recovering a TI LP-EM-CC2755P10 or other CC23xx/CC27xx LaunchPad when the debugger cannot connect

Read where the chip is in its boot first, then reset it through the debug probe (`xds110reset`), then try cutting the probe's USB power (with `uhubctl` from the Mac, or by unplugging the cable; on the hub used here `uhubctl` most likely did not cut it, and the unplug is not run), then try connect under reset. On a CC27xx chip such as the one on the LP-EM-CC2755P10, do not use a chip erase as a reset: it closes the debug port until a full image is programmed again, and TI's command-line flash tool (DSLite) could not even connect to the chip in that state. If you see `Error -2001`, `Error -615`, `Debug is not allowed based on` or `Unable to authenticate with Secure Debug Manager`, start here.

**Source of the recovery tables:** [data/recovery.json](../data/recovery.json) holds, for every board, how it locks up, the methods in order (remote or hands, proven, failed or untested, with the file and date), the restore path and which hub ports really cut power. `python3 tools/recovery_table.py` writes it into each board folder's `AGENTS.md` and `FAQ.md` under "When the board stops answering"; `tools/preflight_image.py <board id> <image.out>` checks an image before a flash. This page keeps the CC23xx and CC27xx narrative and the commands; it holds no table of methods, so a new result goes in the data file.

Terms: [docs/vocab.md](vocab.md) (SACI, HSM, CCFG, SCFG, AHB-AP, CFG-AP, SEC-AP, boot status, chip erase, GEL, xds110reset, uhubctl). Board answers: [cc2755p10/FAQ.md](../cc2755p10/FAQ.md), [cc2340r5/FAQ.md](../cc2340r5/FAQ.md). Checked 2026-10-09 on macOS 15.7.9 (Intel) with CCStudio 21.0.1, UniFlash 9.6.0 (DSLite 9.6.0.5764), XDS110 firmware 3.0.0.43 and uhubctl from Homebrew, on one LP-EM-CC2755P10. Sizes: 1 kB = 1,024 bytes.

Contents: 1. Read the boot status. 2. Reset through the probe. 3. Power-cycle the probe's USB port. 4. Connect under reset. 5. What a chip erase does to a CC27xx. 6. What needs a hand. 7. Where the board stands.

## 1. Read where the chip is before you change anything

CC23xx and CC27xx chips report a boot status byte through the CFG-AP, even when debugging is closed. Read it first and after every step: it tells you whether the step did anything. From the root of this repo:

```bash
/Applications/ti/ccs2101/ccs/scripting/run.sh tools/recovery/cc2x_status.js cc2755p10/CC2755P10.ccxml
```

The script ([tools/recovery/cc2x_status.js](../tools/recovery/cc2x_status.js)) connects to the debug access port only, never to the CPU, and writes nothing. When more than one XDS110 probe is plugged in, put your probe's serial number in both `LS5600RE` fields of the `.ccxml` (`xdsdfu -e` in `ccs_base/common/uscif/xds110/` lists the serial numbers). For the status the chip last reported, 0x020dba05, the script prints this (printed from that value, not a saved run):

```text
DEVICESTATUS 0x020dba05
boot status 0xba ROM serial bootloader started (normal for a chip with no valid configuration)
debug port (AHB-AP) closed
```

The values seen, with their names from TI's CC27xx technical reference manual (SWCU195A, Table 9-2):

| Boot status | TRM name | What it meant here |
|---|---|---|
| 0x20 | BOOT_ENTERED_SACI | The chip waits for SACI commands. Seen on 2026-10-09 at 10:31, 10:36 and 10:38 with the probe connected; it does not mean a flash tool can connect (the one used here could not) |
| 0x3F | BOOT_FAULT_HANDLER | Stuck until reset. Shown right after four SACI blank-check commands from this repo's scripts timed out; cause suspected, not isolated |
| 0xBA | ROM serial bootloader entered | Seen once, after a reset of a chip with blank CCFG and invalid SCFG |
| 0xC2 | Boot complete, set just before the application starts | Boot finished; if the debugger still cannot connect, look at what the application does |

The script also says whether the AHB-AP, the debugger's path to the CPU, is open.

## 2. Reset through the debug probe

The XDS110 probe can pull the target's reset line. This resets the chip, not the probe.

```bash
/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset -a toggle -d 100 -s LS5600RE
```

`-s` is the probe's serial number; `-d` is how long reset is held, in milliseconds (`xds110reset -h`). In one case on 2026-10-09 it took the chip from the stuck 0x3F to 0xBA. It did not bring the same chip back earlier that day, after TI's `zigbee/rf_test` example had left the debugger unable to connect (observed 2026-10-09, log not saved); why is not known.

## 3. Power-cycle the probe's USB port with uhubctl

An LP-EM board on an LP-XDS110 or LP-XDS110ET probe with the TGT VDD jumper on XDS gets its only power from the probe, so cutting the probe's USB power should restart the board. `uhubctl` switches one port of a hub that supports per-port power switching. Install it with `brew install uhubctl`.

Find the hub and port: run it with no arguments and look for the probe's serial number.

```text
uhubctl
```

A line `Port 4: ... XDS110 ... LS5600RE` under `hub 20-2` means location `20-2`, port `4`. Then cycle that one port, off for 5 seconds:

```text
uhubctl -l 20-2 -p 4 -a cycle -d 5
```

Always give `-l` and `-p`; without them it switches every port of the default hub, including other boards. Wait about 6 seconds for the probe to come back.

What to watch for, from the uhubctl README (github.com/mvp/uhubctl, read 2026-10-09) and from this bench:

- **Not every hub really cuts power.** The README: "while most hubs will cut off data USB connection, some may still not cut off VBUS to port." The probe drops off USB and comes back either way, so that proves nothing. Read the boot status before and after. On the Genesys Logic hub used here (`05e3:0610`, USB 2.1, 4 ports) a 5-second cycle left the chip stuck at 0x3F, so the board most likely kept its power.
- **Some hubs switch every port together.** The README lists a hub where one port controls all of them. A cycle there resets every board on the hub.
- **A USB 3 hub is two hubs**, a USB 2 half and a USB 3 half, and power only drops when both switch the port off. uhubctl does both unless you pass `-e`; on that hub it switched the companion hub `1-2`, port 4, too.

If the boot status does not change, unplug the probe's USB cable for 10 seconds (section 6).

## 4. Connect under reset

Connect under reset holds the chip in reset while the debugger attaches, so an application that sleeps or turns off the debug pins never runs. Tried on the LP-EM-CC2755P10 after `zigbee/rf_test`, it gave `The target failed to see a correctly formatted SWD header (Error -615)` (observed 2026-10-09, log not saved). One try, so it shows only that it did not help in that case.

## 5. What a chip erase does to a CC27xx (Error -2001, "Unable to authenticate with Secure Debug Manager")

CCStudio ships a chip erase for these chips that works without the CPU: the GEL function `ChipErase()` in `ccs_base/emulation/gel/cc23xx_dap.gel`, run on the `CS_DAP_0` core. It sends SACI's chip erase command. Its comment says it is for "a chip that has been configured (CCFG) to disable the debug port".

On a CC27xx (CC2745, CC2755) the erase clears main flash, CCFG and SCFG, keeping only the main-flash sectors the old CCFG listed to retain. Running this erase on the LP-EM-CC2755P10 to clear a connect error locked the debug port; read the TRM sections quoted here first. On CC27xx the script always asks to retain the sectors the old CCFG lists, and the board's shipped CCFG and the SDK example checked list the top 96 kB, the HSM firmware. The TRM, section 9.1.3: "Debug access is only enabled if the XCFG (CCFG and SCFG) regions are valid." After the one erase:

- `dslite --mode flash ... -N` (list cores) failed with `Debug is not allowed based on`, then `GEL: Error while executing OnPreTargetConnect(): Unable to authenticate with Secure Debug Manager!`, then `Error connecting to the target: (Error -2001 - (0:20:0)) Internal error: Invalid parameter passed to function.` A flash with this repo's restore script stopped the same way. Not retried from the UniFlash or CCStudio windows.
- Read-only SACI checks said the CCFG is blank and the CCFG and SCFG checksums do not match (invalid). The HSM reported that it failed to boot. Whether its firmware is still in flash is unknown: with invalid CCFG and SCFG it would not be expected to boot anyway.
- Suspected cause of the error: TI's connect script hands every connection to a closed AHB-AP to the Secure Debug Manager (`cc23xx_xds.gel`, `OnPreTargetConnect`), and the TRM says debug access is only enabled when CCFG and SCFG are valid.

A second erase removes the HSM firmware too. TRM section 9.2.2: "a sequence of (chip erase → reset → chip erase) erases both CCFG and non-retained MAIN sectors the first time around and CCFG and all MAIN sectors the second time around." The F3 SDK's `bin/hsm/README.md` says the same, and TI's CC27xx Security User's Guide (HSM overview, read 2026-10-09) adds that a chip erase resets the HSM firmware's anti-rollback protection.

The way back that TI documents is TRM section 9.2.4.1, "Initial Programming of a New Device": over SACI, with no CPU connection, program main flash, then the CCFG sector, then the SCFG sector, then reset. SEGGER says its J-Link does this when it connects to a CC2745 with an empty configuration (kb.segger.com/TI_CC2745, read 2026-10-09); a J-Link was not run. Then, in UniFlash, Settings and Utilities, Read HSM System Information, and if the firmware is missing, Program HSM Image with the `.bin` from the F3 SDK's `bin/hsm/` (do not use the SDK's `CC2745R10_Q1_hsm_fw_v3.5.0.bin` on the LP-EM-CC2755P10: the chip's own HSM firmware is 4.7.6, newer than that file). The anti-rollback reset matters only if a newer HSM firmware had been programmed: after an erase the chip accepts older versions again. TI's SDK pages say HSM programming fails unless a valid image with CCFG and SCFG is already on the chip.

Worked 2026-10-09: `cc2755p10/recovery/saci_program.js` (check, then write); skip the chip erase on an already blank chip.

On CC23xx chips (for example the CC2340R5) the same erase clears main flash and CCFG; there is no SCFG. Not run on a CC23xx.

Before any erase on a CC27xx, back up main flash, the CCFG (0x4E020000, 2 kB) and the SCFG (0x4E040000, 1 kB), and make sure the restore image carries all three: a DSLite flash starts with a chip erase that also clears the SCFG, so an image without one would leave the chip locked. [cc2755p10/restore.sh](../cc2755p10/restore.sh) builds such an image; it has not been run since the SCFG was added.

## 6. What needs a hand

The hands-on methods for every board, with their status and evidence, are in `data/recovery.json` and in each board's "When the board stops answering" section. For the CC27xx and CC23xx boards they are: unplug the probe's USB cable for 10 seconds (not run), re-seat the probe, check the jumpers (TGT VDD on the probe), and a pin-triggered ROM bootloader (a button held during reset; works only if the CCFG turned the pin trigger on).

## 7. Mistakes to avoid, and where the board stands

- Do not chip-erase a CC27xx to clear a connect error (section 5). Read the chip's boot and debug-access sections in its TRM before running any vendor erase or unlock script.
- Give a restore image main flash, CCFG and SCFG; one without SCFG leaves the chip locked (section 5).
- Four SACI blank-check commands, three of them over the HSM area, were followed by a boot fault (section 1). Do not point SACI verify commands at the HSM area.

The LP-EM-CC2755P10 this page came from is recovered, boot 0xC2: the SACI programming in section 5 restored the shipped image (read back identical) and the HSM firmware, 4.7.6, was intact. Note: the `cc2x_status.js` command needs an absolute `.ccxml` path; a relative one fails.

## A port cycle that left the probe off the bus (2026-10-09)

The LP-AM13E230's XDS110 (hub 20-2, port 2) stopped answering DSLite (Error -260) and the hub could no longer read its name. `uhubctl -l 20-2 -p 2 -a cycle -d 5`, then off for 15 s and on again, left the port at "power connect" without "enable": the probe never re-enumerated. This hub (GenesysLogic 05e3:0610) reports per-port power switching, but on port 4 earlier the board behind it did not reset either, so it most likely does not really remove power. A probe in this state needs the cable pulled by hand.

## A deep-sleep image that no debugger could reach (2026-10-21)

An MSP430 port test on the LP-AM13E230 put the chip in STANDBY0 with the watchdog interval timer as the only wake source. It never woke, and every connect failed (DSLite -613, -6310, -6311; OpenOCD -613). `xds110reset`, a `uhubctl` power-off of the probe port for up to 150 s and connect-under-reset did not recover it; the chip needs the USB cable pulled. The firmware was backed up first (`main-a.bin` and `main-b.bin` identical, plus a restore hex parsed back identical), so after the replug, flash that hex and read the first 64 bytes back. On the LP-MSPM0L1306 the same kind of test recovered: a `uhubctl` power-off did power-cycle that chip. Rule: before any deep-sleep image, name a wake source the SDK documents for that state, keep a button wake and a 5 s awake window at boot, and take the backup first (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`).

Later the same day, after a USB replug that did not help (the probe enumerated, the chip still did not answer), three more remote tries also failed:

- Upstream OpenOCD (built 2026-10-21, cmsis-dap driver, 100 kHz and 1 MHz) reads the debug port ID (DPIDR 0x6ba02477) and reports "stalled AP operation, issuing ABORT"; the abort clears the debug port, but `dap init` still fails with every access port stalled. TI's scripting tool stops one step earlier (CS_DAP connect, Error -613), so the security mailbox, where CCStudio's GEL defines a wait-for-debug command (`DSSM_WAIT_FOR_DEBUG`, 0x0206, in `am13_cs_dap_init.gel`), cannot be reached either.
- 80 fast connects in a row (about one a second) never found a moment when the access ports answered, so the image does not wake on a schedule.
- A `uhubctl` power-off of the probe's own port on a VIA hub (`2109:2822`, location 20-1.4.4, port 4) for 15 s: the hub reported the port off and the neighboring port kept running, but the chip stayed stalled, so the board most likely kept power.

What worked, by hand, on 2026-10-24: hold S5 (BSL invoke), tap S1 (nRST), release S5. The USB replug (above) did not. The status, hub ports and restore path are in `data/recovery.json` (the AM13E230 entry).
