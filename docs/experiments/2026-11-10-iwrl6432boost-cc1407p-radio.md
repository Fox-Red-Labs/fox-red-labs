<!-- experiment
id: E323
board: IWRL6432BOOST
date: 2026-11-10
status: waiting
reached: link
line: The IWRL6432BOOST radar chip's Cortex-M4F can host TI's CC1407P radio driver: a transmit app builds for it on a Mac (46.8 kB boot image, 23.7 kB of RAM), with the radio on the board's SPI A pins, but nothing has been flashed or sent, because the radar's shipped flash image cannot be read back and the eight wires are not on.
stop: unknown
-->
# Can the IWRL6432BOOST radar host the BP-CC1407P-9-FE sub-1 GHz radio, so one board senses and transmits?

**Short answer.** Not run yet. A transmit app for the radar's M4F builds with TI's host driver and this repo's bare-metal layer, and every wire is traced to a TI document except one pin-1 marking. The flash write and the packet count wait on a confirmed restore path and on eight wires.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Code and wiring: [iwrl6432boost/cc1407p](../../iwrl6432boost/cc1407p/README.md).

**Question.** A roadside speed sign needs a radar and a radio. Can the radar board's own processor drive TI's CC1407P radio over SPI, with no second MCU, and reach a CC1354P10 hub indoors?

**Vendor position.** TI's CC1407P BoosterPack guide (SWRU633A, 2026-10-15) says its TRX host driver "can be ported to other MCUs" and lists LaunchPad hosts; the radar board guide (SWRU596) lists the J8 and J9 BoosterPack connectors "for the connectivity option with other TI LaunchPad boards". Neither names this pairing, and TI ships no example of it.

**What was tried.**

- **Wiring.** Read from TI's PROC117B schematic (SWRR180) and SWRU633A. The chip's SPI A port (pads AG, AI, AJ, AH: balls D10, B12, C11, D11) and the interrupt pad AV (J10, GPIO5) reach the SPI header J2 and the sockets J8 and J9. The reset line has no J2 pin; the only free output on a header is the nERROR pad (K11, GPIO4) on J9 pin 1. J8 and J9 give no 3.3 V out as built, so power comes from J2 pin 1. The table is in the folder's README.
- **Firmware.** Seven `hb_*` functions on MMWAVE-L-SDK 05.05.04.02: McSPI A polled at 1 MHz, GPIO, pinmux, ClockP. Chip select is the McSPI's own CS0 (pad D11 has no GPIO function) held with the channel's FORCE bit; MISO is read as a level by flipping pad C11 to GPIO1 while chip select is low. The SDK's `MCSPI_open` already writes SYSCONFIG with smart idle off (SIDLEMODE no idle, AUTOIDLE off), and the app prints the register so a run shows it.
- **Build.** The SDK's `empty` no-RTOS example folder, with the SDK's makefile and the Unix helpers passed in, as for the people-counter build in `portability/radar-family/`.

**How far it got.** Compile, link and the boot image. Not flashed.

| Measure | Value |
|---|---|
| Boot image | 46.8 kB |
| Code and constants | 41.4 kB |
| Initialized data | 1.6 kB |
| RAM (bss) | 23.7 kB |
| Packets received | not run |

Rows: `host_fit` and `packet_loss_1h` in `data/results/iwrl6432boost.csv`.

**The line.** Not reached. Two things stand in front of the first packet.

- **The radar's flash cannot be backed up.** TI's flash tool (`arprog_cmdline.py`) has write, erase and format commands and no read command, and no other tool was found that reads the 16 Mbit serial flash back. Writing the transmit image replaces the shipped demo. The restore is TI's public prebuilt demo image, flashed the same way; the chip's ROM flasher is not touched. This is a write to a board that does not read back, so it is held until the restore path is confirmed.
- **The eight wires are not on**, and the LP-CC1354P10-1 receiver is off USB.

**Things that went wrong on the way, with the fix.**

- `Missing argument: --board or --device must be specified` from SysConfig on the first build: the `.syscfg` file had no `@cliArgs --device "XWRL64XX" --package "FCCSP" ...` header. Keep the header the SDK's example files carry.
- `python3 tools/preflight_image.py iwrl6432boost <image>.out` printed `REFUSE (c) segment load range 0x00400000-0x00400140 is outside this chip's flash and SRAM` for every segment, and `REFUSE: do not flash this image to this board`. The tool assumes an MSPM0-style map (flash at 0x0, RAM at 0x20000000) because `data/boards.csv` has no memory map for this board. This chip's ROM loads the image into M4F RAM at 0x00400000, and the load ranges are the ones TI's own linker file gives every example. The refusal is not about the image; it also means the preflight is no gate for this board yet.
- No picture of the bench was taken before the wiring plan was written; the plan rests on the board documents alone.

**What happens next.** Once the restore path is confirmed and the wires are on: flash the image (S1 position 1 OFF, NRST, `arprog_cmdline.py ... -t META_IMAGE1`, S1 position 1 back ON, NRST), read the radio's version line on the radar's serial port, count packets at the CC1354P10 over at least 5 minutes, then the second build that keeps the radar demo running and sends one packet per frame.
