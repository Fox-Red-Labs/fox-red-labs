# What goes wrong with the used TI CC3200-LAUNCHXL LaunchPad development kit on a Mac, and how do you fix it?

Problems hit with a used CC3200-LAUNCHXL Rev 3.2 (CC3200 chip: Arm Cortex-M4 application core plus a network processor) from a Mac on 2026-10-06. Terms: [docs/vocab.md](../docs/vocab.md). Versions: xPack OpenOCD 0.12, UniFlash 9.6 (DSLite), CCStudio 21.0.1.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Third-party behavior | macOS or open-source code, not TI |
| Unknown | Not found |

## Connecting

### Nothing enumerates on USB and no LEDs light when you plug the CC3200-LAUNCHXL in. What now?

Plug it into a different USB port. On the first plug-in nothing enumerated and no LEDs lit; a different port fixed it. Source: `cc3200-launchxl/README.md`.

### OpenOCD opens the debugger but the JTAG scan chain reads all ones and SWD says "cannot read IDR". Is the chip dead?

Not known. Suspected: the chip itself (pre-production silicon, marked `XCC3200HZ`, or damage from earlier rewiring) or the SOP pins, which look bent over in a close-up photo. All ones on JTAG means nothing is driving TDO back. The same result came with JTAG and SWD as received, held in reset at 500 kHz, and with the wire and SOP jumper removed (SOP 000, 4-wire JTAG, jumpers J8 to J11 on). Next to try: check that no two SOP pins touch and straighten them, then UART flashing with SOP 100 in UniFlash's `cc32xx` mode, which talks to the ROM bootloader instead of JTAG. Source: `cc3200-launchxl/README.md`.

### UniFlash says "No device driver was specified for IcePick_C". What driver do I need?

None. UniFlash 9.6 has no driver for this debugger with the CC3200's JTAG router. Tried with a hand-written ccxml using the Stellaris ICDI connection and the CC3200 device. Source: `cc3200-launchxl/README.md`.

### CCStudio 21.0.1 has no CC3200 in its device list. How do I build for it?

It cannot be built in CCStudio 21.0.1 as installed. The device list has CC3220 variants only. Source: `cc3200-launchxl/README.md`.

## Serial

### No serial port appears on the Mac for the CC3200-LAUNCHXL. Where is the console?

macOS's built-in FTDI driver matches only FTDI's own vendor ID, and this board uses TI's (0451:c32a), so no console appears. Reading it needs a user-space USB library (pyftdi or libftdi) told about 0451:c32a. That was not run. Source: `cc3200-launchxl/README.md`.

### Two CC3200 LaunchPad development kits are plugged in and you cannot tell which is which. Why?

Because every CC3200 LaunchPad reports the serial `cc3200`, so two of them cannot be told apart on one computer. Source: `cc3200-launchxl/README.md`.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

No lockup is recorded for this board.

<!-- recovery:end -->
