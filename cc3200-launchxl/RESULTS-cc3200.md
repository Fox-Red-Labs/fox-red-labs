# What has the used TI CC3200-LAUNCHXL LaunchPad development kit measured?

**Nothing: the chip does not answer the debugger, so every test is failed or not run.** The rows and their reasons are in `data/results/cc3200.csv`; the story is in the [README](README.md) and [FAQ](FAQ.md). Terms: [docs/vocab.md](../docs/vocab.md).

Speed run, 2026-10-17. Goal: show the same bench and AI tools reach an older TI Wi-Fi part (FTDI debugger, old SDK) the way they reach the CC35X1.

## Expectations, written before any measurement

- Console: the FT2232D's second interface is the CC3200 UART; pyftdi opens it at 921600 baud (FT2232D divisor gives 923077, 0.16% off).
- ROM bootloader with SOP 100: answers the break condition with ack 00 CC; version packet returns a CC3200 chip id; storage list shows serial flash. Likelihood it answers: 70% (JTAG silence points at the debug path or SOP pins, not necessarily a dead chip).
- Serial flash backup: SFLASH raw read, two passes, identical hashes.
- Blink: the SDK blinky is about 3.2 kB of code, loaded to SRAM at 0x20004000 from /sys/mcuimg.bin; flash over UART in under 30 s.
