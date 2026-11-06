<!-- experiment
id: E274
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: waiting
reached: boot
line: With SW3 all OFF and the new 119 GB card, the AM62L runs the ROM and the first-stage loader (BL1) from the SD card and then stalls: no SPL banner, no U-Boot, no Linux, repeating every few minutes. The HMI shootout, the PlayStation controller test and Doom did not run.
stop: Needs a long J17 power-off at the board; no Linux to run anything on.
-->
# Why does the AM62L not boot Linux from the SD card?

**Short answer.** It gets as far as TF-A BL1 and stops. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**Console after the SW3 change** (115200 baud): a run of `C` (the ROM ping), then `NOTICE: BL1: v2.14.0(release):12.00.00.06`, `dram_size: 0x80000000`, `DDR init done`, `ENTERING WFI - end of bl1`, then a hex dump line and nothing else. The same block repeats five times in 50 minutes (about 280 s, 700 s apart at first). No `U-Boot SPL` line, no login. BL1 running means the ROM read `tiboot3.bin` from some source and DDR trained; the next stage never printed.

**Expectation (before the run).** Linux login within about a minute of power-up, as the as-shipped setting boots from the card (user guide tables 2-5 to 2-9).

**Known twin.** UART boot after `jailhouse disable` stopped at the same place (tiboot3 accepted, SPL silent for 330 s) and a full power cycle cleared it (see the [Jailhouse page](2026-10-22-am62l-jailhouse.md)). Not known whether this is the same fault. Cause not found; other candidates: SW4 or SW2 not at the shipped values (the user guide lists them), card not read.

**Not run.** Boot time to login (`sd-boot-to-login`): it needs a person at the board to power-cycle it. Copying `/root/hidps` from the stick, the PS4 controller, LVGL fbdev and DRM, Slint or Qt software, Chocolate Doom and RetroArch: all need a booted Linux. The JTAG route to read the A53 state was not run.

Rows: `linux_boot_time` `sd-boot-to-login` stays waiting.
