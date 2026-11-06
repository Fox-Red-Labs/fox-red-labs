# What should an AI coding assistant know before working on a used TI CC3200-LAUNCHXL LaunchPad development kit?

That on the used board the debugger opens but the chip never answered, so nothing has been built, flashed or run on it. This file is the short list of what was tried and what is not run. Each item comes from runs on one board.

Terms: [docs/vocab.md](../docs/vocab.md) in the Fox Red Labs repo. In short: **JTAG** and **SWD** are two debug wiring schemes; **SOP** is the three-pin header that sets the boot mode; **OpenOCD** is an open-source debug tool; **UniFlash** is TI's flashing tool. Checked 2026-10-06 on a Mac with xPack OpenOCD 0.12, UniFlash 9.6 (DSLite) and CCStudio 21.0.1, on one used CC3200-LAUNCHXL (`cc3200-launchxl/README.md`).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `cc3200-launchxl/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Kit | CC3200-LAUNCHXL Rev 3.2 silkscreen, bought used (`cc3200-launchxl/README.md`) |
| Chip | Marked `XCC3200HZ`, `447`. The leading X marks a pre-production part; 447 reads as 2014, week 47 |
| Debugger | Onboard FTDI FT2232D, USB 0451:c32a, manufacturer "FTDI", serial `cc3200` for every board of this kind |
| JTAG jumpers | J8 to J11 (TCK, TMS, TDI, TDO) all on |
| As received | A jumper wire ran from the left header row to the SOP header and a jumper sat on SOP; both were removed. A previous owner's program toggled LEDs D5 to D7 at power-up |

## Start here

Nothing works on this board. The debugger opens, the chip never answered, so nothing was built, flashed or run. Tools tried: xPack OpenOCD 0.12, UniFlash 9.6 (DSLite), CCStudio 21.0.1. Probe: the onboard FTDI debugger over USB, nothing to add; JTAG jumpers J8 to J11 on.

- Backup: No tested backup command. Nothing can be read while the chip does not answer.
- Build: not possible. CCStudio 21.0.1 lists CC3220 variants only, no CC3200.
- Flash and check: not run. The debugger opens with `board/ti_cc3200_launchxl.cfg`; the JTAG scan chain reads all ones.
- Restore: not tested.

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Nothing enumerates, no LEDs | Unknown; a different USB port fixed it | Try another port | The FTDI device appears on USB |
| Scan chain reads all ones (JTAG) | Nothing drives TDO back; chip or SOP pins suspected, not confirmed | Check that no two SOP pins touch; the README lists the rest | Scan chain shows an ID |
| `cannot read IDR` (SWD) | Same as above | Same as above | OpenOCD names the chip |
| `No device driver was specified for IcePick_C` | UniFlash 9.6 has no driver for this debugger with the CC3200's JTAG router | None found | None |
| No serial console on a Mac | The built-in FTDI driver matches only FTDI's vendor ID; this board uses TI's | Use pyftdi or libftdi told about 0451:c32a (not done here) | A serial port appears |

Traps shared with other boards: TRAPS.md.

## How to reach it

A different USB port was needed before the board enumerated at all. No serial console appears on a Mac: the built-in FTDI driver matches only FTDI's own vendor ID and this board uses TI's. Reading the console needs a user-space USB library (pyftdi or libftdi) told about 0451:c32a (`cc3200-launchxl/README.md`, "On a Mac"). This was not done.

The debugger opens in OpenOCD with the config the xPack package ships:

```text
board/ti_cc3200_launchxl.cfg
```

## What ran here

- Nothing built, flashed or measured. Blink build: not run, because CCStudio 21.0.1 has no CC3200 entry (CC3220 variants only) (`data/results/cc3200.csv`, row `blink_build`).
- Time to blink: failed. JTAG scan chain reads all ones; SWD reports "cannot read IDR" (`data/results/cc3200.csv`, row `time_to_blink`).
- The radio rows (airtime, energy, throughput, range, flash use) are all not run (`data/results/cc3200.csv`).

## Not run

From the README, in order: check that no two SOP pins touch; try UART flashing (SOP 100) with UniFlash's `cc32xx` mode, which talks to the ROM bootloader instead of JTAG (needs the serial console first); if the ROM answers, the chip is alive and the JTAG path is the problem.

## Where the evidence lives

`cc3200-launchxl/README.md` and `data/results/cc3200.csv`. The board is parked until the cause is known.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

No lockup is recorded for this board.

<!-- recovery:end -->
