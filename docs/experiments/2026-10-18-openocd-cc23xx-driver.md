<!-- experiment
id: E206
board: LP-EM-CC2340R5, LP-EM-CC2755P10
date: 2026-10-18
status: waiting
reached: verified
line: On the LP-EM-CC2340R5, upstream OpenOCD with the new cc23xx driver programmed and verified TI's blink in 0.37 s and the LED pin toggled; a 511 kB image took 10.5 s to write and read back identical. An SDK hex fails verify on the 128-byte CCFG user record, and the CC2755P10 path has not run.
stop: tools; the LP-EM-CC2755P10 was away from the bench, and an SDK hex's CCFG user record differs from a programmed chip's
-->
# Can upstream OpenOCD flash a TI LP-EM-CC2340R5 or LP-EM-CC2755P10 through the XDS110, the way it flashes an MSPM0?

Yes on the LP-EM-CC2340R5: `openocd -f board/ti_lp_em_cc2340r5.cfg -c "program blink.hex verify reset exit"` programmed TI's blink in 0.37 s with a new `cc23xx` flash driver. The CC2755P10 path builds and has not run. Code, cfg files and build steps: [ports/openocd-cc23xx/](../../ports/openocd-cc23xx/README.md).

Terms: [docs/vocab.md](../vocab.md), [LP-EM-CC2340R5 FAQ](../../cc2340r5/FAQ.md), [LP-EM-CC2755P10 FAQ](../../cc2755p10/FAQ.md). SACI is the chip's device management interface on its security access port. 1 kB = 1,024 bytes.

| Step | LP-EM-CC2340R5 | LP-EM-CC2755P10 |
|---|---|---|
| Identity and a 512 kB dump identical to DSLite's | Works | Not run |
| Reset through SACI, halt in the boot ROM | Works, 17 ms | Not run |
| Erase and write one 2 kB sector | Works, 10 ms and 34 ms | Not run |
| `program ... verify reset` of TI's blink | Works, 0.37 s, LED pin toggles | Not run |
| Put a 511 kB image back and read it identical | Works, 10.5 s plus 0.9 s verify | Not run |

**Question.** Can OpenOCD built from git program a CC23xx or CC27xx LaunchPad development kit over the XDS110's CMSIS-DAP interface, with no CCStudio or UniFlash?

**Vendor position.** OpenOCD master (2026-10-16) has target files for CC13x0, CC13x2, CC26x0, CC26x2 and CC32xx and none for CC23xx or CC27xx. TI's CC27xx TRM (SWCU195A, section 9.2.3) lists two SACI erase commands, `SACI_CMD_FLASH_ERASE_CHIP` and `SACI_CMD_FLASH_ERASE_MAIN_APP` ("Erases all (non-retained) MAIN sectors"), and `SACI_CMD_FLASH_PROG_MAIN_SECTOR`, which programs "all or a part of one MAIN sector". The SDK's `driverlib/hapi.h` gives the CPU a ROM call that erases one sector, `HapiFlashSectorErase(key, addr)`.

**What was tried.** OpenOCD master `bb5a52da` with Homebrew's autoconf, automake, libtool, pkgconf, libusb, hidapi and jimtcl, on macOS 15.7.9 (Intel). The driver (`ports/openocd-cc23xx/src/cc23xx.c`, about 930 lines) does the following:

- It identifies the chip by DEVICEID and PARTID from the configuration access port and refuses unknown parts.
- It resets through SACI (RSTCTL 0x5C, the debug-open check, EXIT_SACI_HALT, a halt in the boot ROM's wait loop), as TI's CCStudio scripts do.
- It erases and writes with the boot ROM's `HapiFlashSectorErase` and `HapiFlashProgram` on the halted CPU.
- It never sends a chip erase, refuses the HSM area (0xE8000 and up on the CC27xx), and lets a CCFG or SCFG section through only when it equals the chip's.
- It leaves the debug port dormant when OpenOCD exits.

The LP-EM-CC2340R5 was backed up twice with DSLite first, then run through the steps in the table, then put back.

**How far it got.** Verified on the LP-EM-CC2340R5. The blink's red LED pin (DIO14) read on, off, on at about 1 s steps through the debugger. The board's own image went back and read identical with the CPU held before it started. After it booted, DSLite read 139 bytes changed in the last sector: records the image writes to its settings sector at start.

**The line.**

- SACI cannot erase one 2 kB sector, so erase and write go through the CPU and the boot ROM. A chip whose CCFG closes debugging cannot be flashed this way.
- PROG_MAIN_SECTOR does not erase first, measured: 16 bytes of 0xF0 programmed over 0x0F read back 0x00, and the rest of the sector kept its 0x0F.
- A TI SDK hex fails `verify` in 128 bytes: SDK builds fill the CCFG user record with 0x00, a programmed chip has it blank, and the driver does not write it. Main flash is correct. A hex with main flash only verifies.
- OpenOCD's exit puts the SWD port in JTAG state. TI's DSLite then failed with `Error -615 ... The target failed to see a correctly formatted SWD header`. The driver's `cc23xx dormant`, run before shutdown, fixed it.

There is no TI example to start from. The nearest sources were TI's CCStudio GEL scripts (`cc23xx_dap_saci_util.gel`, `cc23xx_xds.gel`, `cc23xx_util.gel`), the two TRMs, the SDK's `hapi.h` and `flash.h`, and this repo's SACI scripts (`cc2755p10/recovery/saci_program.js`, `site/flash/cc-saci.js`).

| What went wrong | Cause | Cost | Fix |
|---|---|---|---|
| The plan assumed a SACI sector erase | **Tool or docs**: the TRM lists no such command and does not say PROG_MAIN_SECTOR leaves the sector unerased | About 30 min of reading | Erase and write through the boot ROM; SACI for reset and CRC |
| `program` of TI's blink hex failed `verify` | **Tool or docs**: the SDK hex's CCFG user record is 0x00, the chip's is 0xFF | One extra run | Program main flash only, or the chip's own CCFG |
| DSLite could not connect after OpenOCD (`Error -615`) | **Third-party behavior**: OpenOCD leaves an SWD port in JTAG state; TI's tools expect it dormant | One failed restore attempt | `cc23xx dormant` before shutdown, in the target files |

Effort: about 3 hours, about 15 OpenOCD and 16 DSLite sessions on one board, no hands needed, two Homebrew packages added.

Verdict: good on the CC2340R5. The whole program-verify-reset cycle for a blink is under half a second, and a full 511 kB image takes about 11 s. The CC27xx needs its first run.

1. The same gates on the LP-EM-CC2755P10 (probe LB07021W): its debug check is REQ_KEY_ID, and it adds the SCFG and HSM banks.
2. Write the CCFG user record when the chip's is blank and the image's is not, so an SDK hex verifies as is (a one-time write: the record cannot be changed afterwards without a chip erase).
3. A SACI-only path for a chip whose CPU port is closed: ERASE_MAIN_APP, then PROG_MAIN_PIPELINED, with the CCFG left in place.
4. Send the driver upstream to OpenOCD after the CC27xx run.

**Ask.** Has anyone driven EXIT_SACI_HALT on a CC27xx from a debugger other than CCStudio, and did the CPU stop at boot status 0xC1?

Sources: TI SWCU195A (CC27xx TRM) sections 9.1.3, 9.2.3, 9.2.4, 9.3.2; TI SWCU193A (CC23xx TRM) section 8.3.2.2; CCStudio 21.0.1 `ccs_base/emulation/gel/cc23xx_*.gel`; SimpleLink Low Power F3 SDK 9.21.00.36 `driverlib/hapi.h`, `flash.h`; OpenOCD `src/target/adi_v5_swd.c`, `src/flash/nor/rp2xxx.c`. Logs: `ports/openocd-cc23xx/README.md`, test log.
