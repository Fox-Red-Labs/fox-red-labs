# TI TMDS62LEVM (AM62L EVM) from a Mac: errors, causes and fixes

Problems hit bringing up the TMDS62LEVM (AM62L32: two Cortex-A53, 160 kB on-chip SRAM, 2 GB LPDDR4) from a Mac on 2026-10-21 and 2026-10-22. Each answer starts with the fix and quotes the error text. Versions: Apple M4 Mac, CCStudio 21.0.1 (DSLite 21.0.0.4166), XDS110 firmware 3.0.0.32, TI Linux SDK 12.01.00.05.03, Zephyr commit a915b2f. 1 kB = 1,024 bytes.

Cause tags: Setup or handling (how it was plugged in or set); Tool or docs (a tool or missing instruction); Third-party behavior (macOS, Zephyr, TF-A, Linux); Chip limit; Unknown.

### The serial port is silent, or prints junk, though the board is powered. What now?

Read with a program that sets the rate and keeps the port open: `python3 -I am62l-evm/tools/capture.py /dev/cu.usbserial-<id>0 115200 console.log`. `stty -f PORT 115200` followed by `cat PORT` reads at 9600 on macOS, because the setting reverts when `stty` closes the device. Source: `am62l-evm/tools/capture.py`.

### The console stays silent with the switches as shipped. Is the board dead?

No SD card is in the slot, so the boot ROM waits silently on the shipped SD-primary setting. Insert a bootable card, or set UART boot (SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF) and power cycle.

### I changed SW3 and nothing changed. Why?

Unplug and replug J17. The SW7 warm reset does not re-read the boot switches.

### UART boot never shows the ROM's `C`. Which switches?

Set SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF. The user guide lists SW3.4 first, so a row read left to right gives the reverse. Watch FT4232 port A (suffix 0) at 115200; a lone `C` appears about once a second. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### DSLite says "An invalid processor ID has been found". What is wrong?

The connection file lists only the `tixds510cs_dap` driver. Add `cortexA53`, `cortexM`, `etbcs`, `csstm` and `ctset2`; `am62l-evm/tools/AM62L_TMDS62LEVM.ccxml.template` has them. The board is not at fault.

### Does a DSLite connect change the DDR or the PLL?

With CCStudio's stock `AM62L_SK_EVM` board file, yes: its A53 GEL runs `Set_MAIN_PLL0_HSDIV2_CLKOUT_400MHz`, `PD_DDR_PowerUp` and `DDR_Init` on connect. Use `am62l-evm/tools/AM62L_TMDS62LEVM_nogel_board.xml` (empty GEL), which `load_sram.sh` sets up.

### DSLite prints a firmware warning about the XDS110. Do I update it?

The warning says DSLite wants 3.0.0.43 and the probe has 3.0.0.32. Connect, halt and load work with 3.0.0.32; no update was needed. Source: `docs/experiments/2026-10-21-am62l-sram-zephyr.md`.

### DSLite cannot connect to core 1: error -1062. How do I start core 1?

The core is powered down. Start it with a PSCI call from U-Boot: `python3 am62l-evm/tools/psci_stub.py cpu_on 0x1 <entry>`, paste the printed lines at `=>`; `rc = 0x0` means started. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### U-Boot prints "Transfer data timeout" or "Card did not respond to voltage select! : -110". Is the board broken?

The micro-SD card is bad or unseated; the eMMC answered on the same board. Boot over UART and use a USB stick for Linux. The card on hand was the suspect; no second card was tried. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### U-Boot hangs on `usb start`. What now?

Power cycle with the stick already plugged in. `usb start` after hot-plugging a stick into a running board hung U-Boot.

### Zephyr in on-chip SRAM beside Linux fails or breaks Linux. Why?

Linux owns 0x70800000 to 0x7080FFFF (the SCMI mailbox is at its start). WKUP SRAM at 0x707F0000 holds 34 kB of live TF-A code. Reads of MAIN SRAM above 0x70810000 give a bus error from the normal world. Link Zephyr in DDR at 0xE0000000 and boot Linux with `mem=1536M`. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### Zephyr started by PSCI runs at which exception level?

Non-secure EL1. TF-A on this build starts a secondary core there, so build the image for non-secure EL1 with no EL3 code. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### `/dev/mem` will not show Zephyr's ring from Linux.

Boot Linux with `mem=1536M`: the ring at 0xEFFFF000 then lies outside Linux's memory and `/dev/mem` allows the read. `cpuidle.off=1` was set in the run that worked; it was not tested without. Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### The 144 kB Zephyr SRAM image panics at start-up. The 80 kB image will not link.

Give the 144 kB image 8 translation tables (`bare-el3.conf`); 6 panicked in the simulator. The 80 kB MAIN-SRAM map is 24 kB short even with every cut; the floor for hello world is 104 kB. Source: `am62l-evm/zephyr/README.md`.

### The Zephyr build fails in Kconfig with a path error.

The workspace path has a space. Move it to a path without one; a symlink does not help because `west` resolves the real path. Source: `am62l-evm/zephyr/README.md`.

### Zephyr on core 1 stops Linux's interrupts.

Build Zephyr with `patches/gic-skip-distributor.patch` (`CONFIG_GIC_SKIP_DISTRIBUTOR`) so it leaves the GIC distributor alone. Source: `am62l-evm/zephyr/README.md`.

### There is no `west flash` for this board. How do I load an image?

Load from U-Boot (`fatload`, then `go 0x82000000` for `build-ddr`) or over JTAG with `load_sram.sh`; Zephyr's docs say OpenOCD needs a build newer than 0.12.0. Source: `am62l-evm/zephyr/README.md`.

### I send an SGI from Linux to core 1 and Zephyr never takes it.

Use SGI 0 to 7. Writes that enable SGI 8 to 15 in core 1's redistributor (`GICR_ISENABLER0`) are ignored from the non-secure world on this chip. Source: `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md`.

### After a first run, Zephyr on core 1 starts but gets no doorbell.

If the last run ended with PSCI CPU_OFF inside the SGI handler, that SGI stays active and pending in the redistributor (`ISACTIVER0` and `ISPENDR0` read 0x80 through `/dev/mem`). End the run from the main loop, never inside the handler, and clear both bits at start. Source: `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md`.

### How do I build a kernel module on the board?

The board has gcc 15.3 but no kernel headers, flex or bison. Copy the SDK kernel source to the board (about 200 MB without `drivers/*.c`), put the board's `/proc/config.gz` in a build folder with `CONFIG_LOCALVERSION` set so it matches `uname -r` and `LOCALVERSION_AUTO` off, supply `flex` and `bison` as scripts that copy lexer and parser output generated elsewhere, run `make O=... ARCH=arm64 modules_prepare`, then `KBUILD_MODPOST_WARN=1 make ... M=... modules`. `insmod` accepts it because MODVERSIONS is off. Source: `am62l-evm/rpmsg/`.

### The boot ROM takes a 54-block image from `mkphase1.sh` and then pings `C` again.

It refused the image (four certificate variants tried, SoC is HS-FS). It returns to its ping within 3 s. After a transfer cut off part-way it was silent about 4 minutes before pinging. Not resolved. Source: `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md`.

### The AM62L stops after `BL1` and `ENTERING WFI` when booting from the micro-SD card. What now?

Check three things, then power cycle with J17 out for 30 s: SW4 = ON ON OFF OFF OFF OFF ON OFF (not the no-boot setting used for JTAG), SW2 = OFF ON OFF OFF, SW3 all OFF, and the card pushed fully into the slot. With the card half in, U-Boot prints `MMC: no card present` while the boot ROM still read the card for the first stage, so the console shows `C`, BL1 notices and then silence, repeating. After seating the card, `mmc rescan` then `boot` at the `=>` prompt reached the login. Source: `docs/experiments/2026-10-24-am62l-sd-boot-stall.md`.

### What can the AM62L draw on an HDMI monitor with no GPU?

LVGL 9.3 on fbdev drew a fixed 1080p scene at 54 fps (30 percent of both cores), LVGL on DRM at 18 fps, Slint's software renderer at about 42 fps. Use fbdev for LVGL unless you need vsync. Source: `docs/experiments/2026-10-24-am62l-hmi-shootout.md`.

### Chocolate Doom on the AM62L runs at 6 fps. How do I speed it up?

SDL's `kmsdrm` driver, and SDL on Weston, both end in Mesa llvmpipe (software OpenGL) for the scaler: a timedemo ran 6.5 fps on Weston with the pixman renderer and about 6 fps on `kmsdrm`, with two llvmpipe threads at 50 to 80 percent. SDL's software renderer cannot reach `kmsdrm`. Use a port that writes the frame buffer directly (doomgeneric style), as the LVGL fbdev row does. Source: `docs/experiments/2026-10-24-am62l-doom-controller.md`.

### A wired PS4 controller is `Sony Computer Entertainment Wireless Controller` on the AM62L. Does it work?

Yes through `hid-generic`: `/dev/input/event1` reports left stick `ABS_X`/`ABS_Y` (0 to 255, centered near 120, up is a low `ABS_Y`), the face buttons `BTN_SOUTH`/`BTN_EAST`/`BTN_C`/`BTN_NORTH`, and `BTN_MODE` for the PS button. No PlayStation driver is needed for buttons and sticks (rumble, LEDs and the motion sensor need `hid-playstation`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- No bootable SD card: the boot ROM waits silently on the shipped SD-primary setting (`am62l-evm/FAQ.md`)
- After jailhouse disable and reboot, UART boot sends tiboot3.bin but SPL never pings for tispl.bin (330 s, three tries) (`am62l-evm/AGENTS.md`, `docs/experiments/2026-10-22-am62l-jailhouse.md`, 2026-10-22)
- A UART boot sent more than 5 minutes after power gets receiver cancelled; a cut-off transfer leaves the ROM silent for about 4 minutes (`am62l-evm/AGENTS.md`, `am62l-evm/FAQ.md`, 2026-10-24)
- booti without setenv bootargs prints Starting kernel and nothing else (`am62l-evm/AGENTS.md`)
- SD boot stalls after BL1 (console: ROM C, BL1 notices, ENTERING WFI, then silence, repeating) when SW4 is left in a no-boot setting and the card is not pushed in; U-Boot then prints MMC: no card present while the boot ROM still reads the card (`am62l-evm/FAQ.md`, `docs/experiments/2026-10-24-am62l-sd-boot-stall.md`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug and replug J17 (full power cycle); SW7 warm reset does not re-read the boot switches (clears the SPL silence after jailhouse disable; cause not found) | hands | proven | `am62l-evm/AGENTS.md`, `am62l-evm/FAQ.md`, date not recorded |
| UART boot: SW3.4 ON, 3.3 ON, 3.2 ON, 3.1 OFF, power cycle, uart_boot.py, plug J17 (about 7 minutes to the U-Boot prompt) | hands | proven | `am62l-evm/AGENTS.md`, 2026-10-22 |
| reboot from default Linux returns to the ROM ping | remote | proven | `am62l-evm/README.md`, date not recorded |
| Boot from the micro-SD slot: SW4 ON ON OFF OFF OFF OFF ON OFF, SW2 OFF ON OFF OFF, SW3 all OFF, push the card fully in, full power cycle; if U-Boot shows MMC: no card present, seat the card, then mmc rescan and boot (119 GB card with the SDK 12.01 default image reached login after seating the card; the 2026-10-22 timeouts were with a different card) | hands | proven | `am62l-evm/FAQ.md`, `docs/experiments/2026-10-24-am62l-sd-boot-stall.md`, 2026-10-24 |

<!-- recovery:end -->

### I wrote the SDK image to the SD card from Linux and the checksum of the whole card differs from the source.

udev mounts both partitions read-write when the write ends, which changes the ext4 superblock (1 MiB at the start of partition 2). Unmount `/run/media/*mmcblk1*` before the read-back, or compare by chunks and expect one differing megabyte. Source: `docs/experiments/2026-10-24-am62l-sd-card-and-ps5-driver.md`.

### The kernel has no driver for a DualSense or a DualShock 4.

`HID_PLAYSTATION`, `HID_SONY` and `INPUT_UINPUT` are not set. Build `hid-playstation.ko` and `led-class-multicolor.ko` on the board as in the module FAQ above, using the sources from the `ti-linux-6.18.y` branch, then `insmod` both (the second first). A wired DualShock 4 falls back to `hid-generic`. Source: `docs/experiments/2026-10-24-am62l-sd-card-and-ps5-driver.md`.

### whisper.cpp or llama.cpp builds take hours on the board.

Two A53 cores: whisper.cpp about 1 h, llama.cpp about 2.5 h with `-j2`. Stop the build processes with `kill -STOP` while measuring; they cannot run beside a benchmark. Do not `pkill -f` a pattern that appears in your own ssh command line. Source: `docs/experiments/2026-10-24-am62l-edge-ai-cpu.md`.

### The board shell says `syntax error near unexpected token` for a pipe in parentheses.

The board's `sh` is not bash; send plain pipelines or a script. Root ssh works from your computer at the board's DHCP address. Source: same page.

### RetroArch in the showcase ran a Game Boy game at 48 fps (10.4 fps at 1080p). How do I get 60 fps and SNES or PlayStation speed?

Skip X and the container. A libretro frontend that writes straight to DRM/KMS (`am62l-evm/lean/lean_fe.c`) ran the same Gambatte core at 60 fps in both modes, Snes9x 2005 at 60 fps up to 1920x1080 and PCSX-ReARMed at 60 fps on a PlayStation test program. The slowdown was Mesa llvmpipe scaling on two cores, not the emulator. The display plane cannot scale, so scale integer ratios on the CPU. Source: `docs/experiments/2026-11-01-am62l-lean-emulation.md`.
