# Why does the used TI CC3200-LAUNCHXL LaunchPad development kit not answer its debugger?

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**The debugger opens, the chip does not answer, so the board is parked and nothing has been built, flashed or run on it.** A used CC3200 LaunchPad development kit, plugged in 2026-10-06: a Wi-Fi MCU (Cortex-M4 application core plus a network processor). Parked until the cause is known.

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). **JTAG** and **SWD** are two debug wiring schemes; **SOP** is the three-pin header that sets the boot mode.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-cc3200.md` (what was expected, and that nothing has run). Results are rows in `data/results/cc3200.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers; there is nothing to build, because CCStudio 21.0.1 has no CC3200 target and the chip does not answer the debugger.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add cc3200-launchxl tools data
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.

## The board

- Silkscreen: CC3200-LAUNCHXL Rev 3.2.
- Chip marking: `XCC3200HZ`, `447`. The leading X marks a pre-production part; 447 reads as 2014, week 47.
- Onboard debugger: FTDI FT2232D. USB vendor and product 0451:c32a, manufacturer "FTDI", serial number `cc3200`.
- As received: a green jumper wire ran from the left header row to the boot-mode header (SOP), and a blue jumper sat on the SOP header. Both removed for these tests. All four JTAG jumpers (J8 to J11: TCK, TMS, TDI, TDO) are on.
- It runs a previous owner's program: LEDs D5 to D7 toggle at power-up.

## Ten-minute path

Nothing runs: the debugger opens but the chip does not answer, so there is no path to a changed blink. What was tried is under "What was tried" below.

## One-command build

Nothing runs: CCStudio 21.0.1 has no CC3200 entry (CC3220 variants only), so there is no build target, and no image could be loaded because the chip does not answer the debugger.

## What was measured

Nothing measured. Every test that needs the chip is a row in `data/results/cc3200.csv` marked failed or not run, each with its reason; the first run to answer will be UART flashing with SOP 100.

## What was tried (2026-10-06)

| Try | Tool | Result |
|---|---|---|
| First plug-in | USB | Nothing enumerated, no LEDs. A different USB port fixed it. |
| JTAG, as received | OpenOCD 0.12 (xPack), `board/ti_cc3200_launchxl.cfg` | Debugger opens; scan chain reads all ones |
| SWD, as received | same, `TRANSPORT swd` | "cannot read IDR" |
| JTAG and SWD, held in reset, 500 kHz | same | same |
| Wire and SOP jumper removed (SOP 000, 4-wire JTAG) | same | same |
| TI's debug engine | UniFlash 9.6 DSLite, hand-written ccxml (Stellaris ICDI connection, CC3200 device) | "No device driver was specified for IcePick_C": no driver in UniFlash 9.6 for this debugger with the CC3200's JTAG router |
| CCStudio 21.0.1 | device list | No CC3200 entry (CC3220 variants only) |

All ones on JTAG means nothing is driving TDO back. With the jumpers in place, the likely causes are the chip itself (pre-production silicon, or damage from the earlier rewiring) or the SOP pins, which look bent over in the crop.

## On a Mac

- No serial console appears: macOS's built-in FTDI driver only matches FTDI's own vendor ID, and this board uses TI's. Reading it needs a user-space USB library (pyftdi or libftdi) told about 0451:c32a.
- Every CC3200 LaunchPad reports the serial `cc3200`, so two of them cannot be told apart on one computer.

## If you see this

- If you see `scan chain all ones` (JTAG) or `cannot read IDR` (SWD) from OpenOCD, nothing is driving the chip's debug lines back: check that no two SOP pins touch and straighten them ([FAQ](FAQ.md#openocd-opens-the-debugger-but-the-jtag-scan-chain-reads-all-ones-and-swd-says-cannot-read-idr-is-the-chip-dead)).
- If you see `No device driver was specified for IcePick_C` from UniFlash 9.6, there is no driver in UniFlash 9.6 for this debugger with the CC3200's JTAG router ([FAQ](FAQ.md#uniflash-says-no-device-driver-was-specified-for-icepick_c-what-driver-do-i-need)).
- If you see nothing enumerate on first plug-in, no LEDs, try a different USB port ([FAQ](FAQ.md#nothing-enumerates-on-usb-and-no-leds-light-when-you-plug-the-cc3200-launchxl-in-what-now)).

The rest: [FAQ](FAQ.md).

## Limits

JTAG does not reach this board. If you see that, check that no two SOP pins touch, then try UART flashing (SOP 100) with UniFlash's `cc32xx` mode, which talks to the ROM bootloader instead of JTAG.

## Where these facts come from

Checked 2026-10-06 on a Mac with xPack OpenOCD 0.12 (`board/ti_cc3200_launchxl.cfg`), UniFlash 9.6 (DSLite) and CCStudio 21.0.1, on one used CC3200-LAUNCHXL. Files: `data/results/cc3200.csv`, `cc3200-launchxl/RESULTS-cc3200.md`, `cc3200-launchxl/AGENTS.md`, `cc3200-launchxl/FAQ.md`.
