<!-- experiment
id: E174
board: SK-TDA4VM
date: 2026-10-10
status: line found
reached: verified
line: No, not at this bench. From the sealed box to a Linux prompt took about 2 h 30 min of working time, and the first model inference came about 40 minutes after that, about 3 h 10 min in all. Two things took the time beyond the hour on TI's page: the micro-SD card in the box did not boot (inferred to be blank; 62 minutes of work to write it over the serial cable with no card reader at hand), and the board's serial console needs a third-party driver on macOS that the user guide does not mention (about 90 minutes, not timed). With a written card the board reaches its login prompt 27.7 s after the first bootloader byte on its first boot and 24.2 s on later boots, and runs TI's model zoo in minutes.
stop: tools
-->

# Does the SK-TDA4VM reach a first inference in under an hour from the sealed box, with only public documents?

Terms used here are in [docs/vocab.md](../vocab.md) (U-Boot, UART boot, boot ROM, xmodem and ymodem, the model zoo) and the board's [FAQ](../../tda4/FAQ.md). Board page: [tda4/README.md](../../tda4/README.md). The card-writing times were taken with a stopwatch; the other two figures below were observed and not saved.

**Short answer.** No, not at this bench: about 2 h 30 min of work from the sealed SK-TDA4VM box to a Linux prompt and about 40 minutes more to the first model inference. The hour went on two things TI's documents do not mention, a card that did not boot and a macOS console driver; the 62 timed minutes of card writing alone pass the hour, so the verdict does not rest on the two estimates. With a written card the board itself is fast.

**Question.** TI sells the SK-TDA4VM as a kit a developer can start prototyping a vision application on in under an hour. From a sealed box, with a Mac, the public documents and no network cable, how long does it take to see the first model run?

**Vendor position.** TI, SK-TDA4VM product page (read 2026-10-09): the kit "enables prototyping a vision-based application in less than an hour". TI, Edge AI SDK getting-started guide 11.00 (read 2026-10-09): "the board will take less than 20sec to boot and display a wallpaper". The guide's first steps: download the image, write it to a micro-SD card with a card writer, insert the card, connect a display, Ethernet and the USB-C supply, power on. The SK-TDA4VM user guide (SPRUJ21, read 2026-10-09) lists a micro-SD card among the kit contents (wording not saved) and names the board's serial bridge chip and a Windows driver for it; it names no macOS driver.

This repo's expectation before the run: "Yes, if power and card are right; the supply is the likely stop".

**What was tried.** Board: SK-TDA4VM (J721EXSKG01EVM, PROC112A1) with the micro-SD card from its box, an Intel Mac on macOS 15.7.9, a 20 V class USB-C supply, the micro-B console cable. No display, no Ethernet and no micro-SD card reader at the bench. The second sitting (2026-10-09, 21:20 to 22:50) is in the stopwatch and timeline files and the 62-minute figure comes from them; the earlier driver work is in neither, so its 90 minutes is an estimate.

1. Console. Plugged the micro-B cable in; the Mac showed no serial port. Installing the CP210x driver, approving it in System Settings and replugging gave four ports; working out which of the four is the Linux console took more time (the user guide's order did not match the Mac's naming). About 90 minutes in the first sitting, observed 2026-10-09 and not saved; part of it was this repo's own search for the right port.
2. First power-on. Every port silent, a reset changed nothing. Inferred: the card from the box was blank. It was not read before writing, and the chip's boot ROM prints nothing when it finds no image. One kit; whether every kit ships this way is unknown.
3. Writing the card with no reader. Downloaded the image to a second Mac (4 min), pulled the four bootloader files out of it and cut the image into 256 MiB compressed pieces (12 min together), loaded U-Boot (the bootloader that runs before Linux) into the chip over the serial port (UART boot, set by the SW1 switches), tried U-Boot's mode that shows the card to the Mac as a USB disk (it crashed), then put the pieces on a USB stick and wrote the card from U-Boot (5.4 min of writing). 62 minutes of work from the first console check to the Linux login, 11 of them hands-on and 51 waiting, over 81 minutes on the wall clock with the idle gaps (the stopwatch counts a 3-minute question to the person at the bench as waiting; the timeline marks it idle, which would make it 59).
4. First boot: 27.7 s from the first bootloader byte to the login prompt, including the root file system growing to the card.
5. First inference: TI's pose model through ONNX Runtime with the TIDL delegate, from the zoo on the card, about 40 minutes after the login prompt (observed, not saved), most of it reading the zoo's layout and the runtime's Python interface over the serial cable.

**How far it got.** Measured. The first inference ran; the question is the clock.

| Stage | Clock | What took the time |
|---|---|---|
| Box to a working console | about 90 min (not saved) | the macOS driver and finding the right port among four |
| Console check to Linux login | 62 min of work (11 hands-on, 51 waiting; 81 min on the wall) | the card from the box did not boot; no card reader; the serial write path |
| First bootloader byte to login prompt, written card | 27.7 s first boot, 24.2 s after (3.8 s more from a typed `reboot`) | the board itself |
| Login to first model inference | about 40 min (not saved) | learning the zoo and the runtime over a 115,200 baud console |
| Sealed box to first inference | about 3 h 10 min of working time (two of the three parts are estimates) | |

With a card reader, the same path would be the download (4 min), the write (roughly 10 minutes at this image size, an estimate, not measured) and the 24 s boot; the first inference would then be the time it takes to type the runtime example, which is minutes. TI's hour may be reachable on a Linux PC with a reader and a display; that path was not timed. At this bench it was not reached.

**The line.** The stop was tools, not the board. Two gaps in the public path cost the hour:

1. The micro-SD card in the box did not boot, and no document read says the included card needs the image written to it. **Tool or docs.** The user guide says a card is included; the getting-started guide says to write the image but not that the included card needs it. The boot ROM's silence looks exactly like a dead board.
2. The serial console needs a driver on macOS that TI's documents do not mention (CP2108, four ports), and the port order in the user guide does not match what the Mac shows. **Tool or docs.** Some of those 90 minutes were this repo's own search for the right port among the four, which a logger on all four ports would have cut short. **Setup or handling.**

Neither is a hardware fault. Once past them the board was fast: a 24 s boot (to a login prompt with no display; TI's under 20 s is to a wallpaper on a display, a different end point that was not timed), every zoo model running on the accelerator in one evening, hardware video decode at about 450 frames a second on a 263-frame clip with start-up inside the time (about 500 steady, see the zero-copy experiment).

**Ask.** Did your SK-TDA4VM ship with a written card? How long did your first boot take from a sealed box, and on which computer?
