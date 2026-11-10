# How do you bring up the TI TMDS62LEVM (AM62L EVM) from a Mac: console, boot modes, Linux and Zephyr?

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

The TMDS62LEVM is a TI evaluation module for the AM62L32: two Arm Cortex-A53 cores up to 1.25 GHz, 160 kB of on-chip SRAM, 2 GB LPDDR4, 32 GB eMMC, micro-SD slot, 512 Mbit OSPI NOR (user guide SPRUJG8B). The box holds the board only: no power supply, cables or SD card. Failures and fixes: [FAQ](FAQ.md). Notes for an AI assistant: [AGENTS.md](AGENTS.md). 
Terms: [docs/vocab.md](../docs/vocab.md) and the [FAQ](FAQ.md). 1 kB = 1,024 bytes. **U-Boot** is the boot loader that loads Linux; **TF-A** is Arm's secure-world firmware; **PSCI** is the Arm call that starts and stops CPU cores; **DSLite** is TI's command-line debug server; **JTAG** is the debug connection the XDS110 probe uses; **SRAM** here is the 160 kB of memory on the chip, as opposed to the 2 GB LPDDR4 beside it.

Checked 2026-10-21 and 2026-10-22 on an Apple M4 Mac with CCStudio 21.0.1 (DSLite 21.0.0.4166), the on-board XDS110 (firmware 3.0.0.32), TI Linux SDK 12.01.00.05.03 (kernel 6.18.38) and upstream Zephyr at commit a915b2f.

## Start here

| Step | Status | What you get | Where |
|---|---|---|---|
| Console on UART A (J7) | Works | U-Boot and Linux text at 115200 8N1 | [Ten-minute path](#ten-minute-path) |
| Boot over UART with no SD card | Works | U-Boot prompt in about 7 minutes (420 s) | [Linux from a USB stick](#linux-from-a-usb-stick-no-sd-card) |
| Linux 6.18.38 from a USB stick | Works | Login prompt about 21 s after `booti` | [Linux from a USB stick](#linux-from-a-usb-stick-no-sd-card) |
| Zephyr hello world in on-chip SRAM, DDR never initialized | Works | Banner and `Hello World! am62l_evm/am62l3/a53` on UART A, loaded over JTAG | `docs/experiments/2026-10-21-am62l-sram-zephyr.md` |
| Linux on core 0 and Zephyr on core 1, no hypervisor | Works | Zephyr in DDR, started by PSCI from U-Boot; ring read from Linux | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| Linux starts Zephyr on core 1 and the two exchange messages through shared DDR | Works | Echo 4.0 to 5.7 us (56 bytes); steps to move a coprocessor job in [MIGRATION.md](MIGRATION.md) | `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md` |
| Zephyr in on-chip SRAM beside Linux | Not for this chip | TF-A and Linux own the SRAM | [Memory map](AGENTS.md#memory-map) |
| Zephyr in the 80 kB MAIN SRAM alone | Not for this chip | Does not link; 104 kB is the floor | `zephyr/README.md` |
| Boot from the micro-SD slot | Works | A 119 GB card with the SDK 12.01 image reached the login once the card was pushed fully in (2026-10-24); the first card on hand gave `Transfer data timeout` | [FAQ](FAQ.md) |
| Zephyr from U-Boot in DDR (`build-ddr`) | Should work | Builds; not run on the board | `zephyr/README.md` |
| Zephyr as the boot ROM's first-stage payload | Should work | `zephyr/mkphase1.sh` builds the image; not run on the board | `am62l-evm/zephyr/README.md` |
| Jailhouse, TI's flow: root cell, `uart-demo`, `gic-demo` and a second Linux as inmates | Works | `jailhouse enable` 0.27 s; inmates print on UART1; commands in [AGENTS.md](AGENTS.md) | `docs/experiments/2026-10-22-am62l-jailhouse.md` |
| Mosquitto, Node-RED, Pi-hole, Home Assistant Container and Python GPIO | Works | Docker is in TI's SDK image; Home Assistant 557 MiB, 67.7 s to first answer; gpiozero and Blinka toggle LD8 | [linux-apps/README.md](linux-apps/README.md) |

## The board

| Part | Fact | Read from |
|---|---|---|
| Chip | AM62L32: two Arm Cortex-A53 cores up to 1.25 GHz | `AGENTS.md` |
| Memory | 160 kB on-chip SRAM, 2 GB LPDDR4, 32 GB eMMC, micro-SD slot, 512 Mbit OSPI NOR at 0x60000000 | `AGENTS.md` |
| Power | USB-C Power Delivery into J17; the source must offer more than 15 W | `AGENTS.md` |
| Console | J7, an FT4232HL with four serial ports; port A is the SoC UART0 console at 115200 8N1 | `AGENTS.md` |
| Debugger | J8, on-board XDS110, firmware 3.0.0.32; DSLite lists CortexA53_0, CortexA53_1 and SMS_CM4_0_TIFS | `AGENTS.md` |
| LEDs and buttons | LD8 user LED on GPIO0_123, SW7 warm reset, SW5 user interrupt on GPIO0_90 | `AGENTS.md` |
| Not in the box | No power supply, cables or SD card | this page |

## Ten-minute path

1. Power: USB-C Power Delivery source that offers more than 15 W, into **J17**. LD6 (board) and LD5 (PD good, J17) light. Below 15 W the board stays off (user guide SPRUJG8B).
2. Console: micro-USB from **J7** (FTDI FT4232HL, four serial ports) to the Mac. The ports appear as `/dev/cu.usbserial-<id>0` to `...3`; check with `ls /dev/cu.usbserial-*`.

   | Suffix | FT4232 port | Connects to |
   |---|---|---|
   | 0 | A | SoC UART0: U-Boot, Linux and Zephyr console. 115200 8N1 |
   | 1 | B | SoC UART1 |
   | 2 | C | WKUP UART0 |
   | 3 | D | SoC UART4 |

3. Debugger: micro-USB from **J8** (on-board XDS110) to the Mac. `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder prints the probe serial. DSLite prints a warning that it wants firmware 3.0.0.43 and continues.
4. Read the console. Python holds the port open and sets the rate itself (a `stty -f` rate resets when `stty` exits, so `cat` reads at 9600):

   ```bash
   python3 -I am62l-evm/tools/capture.py /dev/cu.usbserial-<id>0 115200 console.log
   ```

5. Set the boot switches (next section), unplug and replug J17 (full power cycle), and watch the console.

## Boot switches

SW3 is BOOTMODE[15:12], SW2 is [11:8], SW4 is [7:0]. ON is 1. The user guide lists SW3.4 first in its tables; the silkscreen numbers each switch, so read it switch by switch. **A new setting needs a full power cycle (unplug J17). The SW7 warm reset does not re-read the switches.**

| Mode | SW3.4 | SW3.3 | SW3.2 | SW3.1 | Other switches |
|---|---|---|---|---|---|
| As shipped: SD card primary, no backup, full pincount | OFF | OFF | OFF | OFF | SW4.1 to SW4.8 = ON ON OFF OFF OFF OFF ON OFF; SW2.1 to SW2.4 = OFF ON OFF OFF |
| SD card primary, UART backup (reduced pincount) | ON | OFF | ON | OFF | SW4 and SW2 inputs ignored |
| UART primary, SD backup (reduced pincount) | ON | ON | ON | OFF | SW4 and SW2 inputs ignored |
| No boot (JTAG load, full pincount) | OFF | OFF | OFF | OFF | SW4.4, SW4.5, SW4.6, SW4.7 all ON |

With no SD card in the slot and the shipped setting the ROM waits silently, so every UART stays quiet. Source: the user guide. It also lists eMMC, OSPI and USB DFU modes in the reduced map; none was run here.

## Linux from a USB stick, no SD card

You need a 32 GB USB stick and TI's Linux SDK image `tisdk-default-image-am62lxx-evm-12.01.00.05.03.rootfs.wic.xz` (1.2 GiB compressed, 14.5 GiB written, sha256 `810bf62d08e9b7eb568fa3fe4f3c4326bdf4aebadb773b1f5a3433201c0f1e04`).

1. Find the stick, check the size matches, unmount it, write it. Ctrl-T prints progress.

   ```bash
   diskutil list external physical
   diskutil unmountDisk /dev/diskN
   xz -dc tisdk-default-image-am62lxx-evm-12.01.00.05.03.rootfs.wic.xz | sudo dd of=/dev/rdiskN bs=4m
   ```

2. Pull the three boot files out of the image (`extract_wic.py` writes `part1.img`, the FAT partition, and `part2.img`, the ext4 root; it leaves zero blocks as holes):

   ```bash
   python3 -I am62l-evm/tools/extract_wic.py tisdk-default-image-am62lxx-evm-12.01.00.05.03.rootfs.wic.xz out/
   hdiutil attach -readonly -imagekey diskimage-class=CRawDiskImage out/part1.img
   ```

   Copy `tiboot3.bin`, `tispl.bin` and `u-boot.img` from the mounted volume, then `hdiutil detach` it.
3. Plug the stick into the board **before** power-up. Set SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF.
4. Start the sender. It holds UART A itself, so do not run `capture.py` on the same port:

   ```bash
   python3 -I am62l-evm/tools/uart_boot.py /dev/cu.usbserial-<id>0 --tiboot3 tiboot3.bin --tispl tispl.bin --uboot u-boot.img --log uart-boot.log
   ```

   It prints `power cycle the board now` and waits up to 330 s for the ROM's ping (a lone `C` about once a second). Unplug and replug J17. Expect `tiboot3.bin` by XMODEM, `tispl.bin` and `u-boot.img` by YMODEM, then the U-Boot prompt `=>` after about 7 minutes (115200 baud moves 11.5 kB/s). Add `--cmds FILE` to feed U-Boot a command file; `--dry-run` prints the plan; `--selftest` needs no board.
5. At `=>` (run 2026-10-22 with SDK 12.01.00.05.03; the kernel file name changes with the SDK version, list it with `ls usb 0:2 /boot`):

   ```
   usb start
   load usb 0:2 ${loadaddr} /boot/Image-6.18.38-ti-00934-g8933766be737-dirty
   load usb 0:2 ${fdtaddr} /boot/dtb/ti/k3-am62l3-evm.dtb
   setenv bootargs console=ttyS0,115200n8 earlycon=ns16550a,mmio32,0x02800000 root=/dev/sda2 rootwait rw
   booti ${loadaddr} - ${fdtaddr}
   ```

   Expect `am62lxx-evm login:` about 21 s after `booti` (median of 3 runs: 20.9, 21.4, 21.7 s). Log in as `root`, no password. Source: `data/results/am62l-evm.csv`, `booti-to-login-median3`.

## Jailhouse hypervisor

TI's Jailhouse flow runs from the SDK 12.01.00.05.03 Jailhouse image: U-Boot loads `Image-jailhouse` and its dtb, then in Linux `jailhouse enable` (0.27 s), `jailhouse cell create` for the `uart-demo` or `gic-demo` inmate, and the inmate prints on UART1 (FT4232 port B). Exact commands: [AGENTS.md](AGENTS.md). Run and numbers: `docs/experiments/2026-10-22-am62l-jailhouse.md`.

## Zephyr on core 1 beside Linux

Image E (`build-ddr-core1`) is 44.0 kB linked at 0xE0000000 with a console ring at 0xEFFFF000. Put `zephyr.bin` on the stick's root partition as `/boot/zephyr-core1.bin`, then at the U-Boot prompt, before booting Linux:

```
load usb 0:2 0xE0000000 /boot/zephyr-core1.bin
```

```bash
python3 am62l-evm/tools/psci_stub.py version                    # prints mw.l lines, then go; expect rc 0x10001
python3 am62l-evm/tools/psci_stub.py cpu_on 0x1 0xE00008BC      # expect rc 0x0
python3 am62l-evm/tools/psci_stub.py affinity 0x1               # 0 = on, 1 = off
```

Paste the printed U-Boot lines at `=>` (each ends with `go 0x88000000`). Then boot Linux with `maxcpus=1 mem=1536M cpuidle.off=1` added to `bootargs`. In Linux, `/sys/devices/system/cpu/online` reads `0` and the ring at 0xEFFFF000 grows (the head counter advanced 66 bytes in 3 s). Source: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

## Zephyr in on-chip SRAM over JTAG (no DDR, no TF-A)

1. Switches: no boot (table above), full power cycle.
2. Edit `RUN`, `SERIAL` and `PORTS` at the top of `am62l-evm/tools/load_sram.sh` to your paths, probe serial and FTDI ports.
3. `am62l-evm/tools/load_sram.sh --list-cores` prints CortexA53_0, CortexA53_1 and SMS_CM4_0_TIFS.
4. `am62l-evm/tools/load_sram.sh --listen --core 0` halts A53_0 in the ROM loop, loads the ELF (one segment at 0x707F0000, entry 0x707F08BC), runs it and captures the four ports. Expected on UART A: `*** Booting Zephyr OS build a915b2f12675 ***` then `Hello World! am62l_evm/am62l3/a53`. The banner arrived 9.8 s after capture start; the 9 s is DSLite connecting and loading.

## One-command build

| Command | Builds | Where it runs |
|---|---|---|
| `am62l-evm/zephyr/setup.sh` | `build-ddr`, `build-sram-main`, `build-sram-all` from nothing, pinned and checksummed | macOS or Linux, x86_64 or arm64 |
| `am62l-evm/zephyr/rebuild_sram.sh` | `build-sram-main` and `build-sram-all` from the overlays | the machine that ran `setup.sh`, about a minute |
| `ONLY=el1 am62l-evm/zephyr/rebuild_core1.sh` | images D and E (without `ONLY`: A to E) | same |

`setup.sh` takes `ZP` (Zephyr folder), `TC` (toolchain folder) and `APP` (application) as environment variables. **Disk: about 3 GB the first time** (Zephyr and four modules about 1.5 GB, toolchain about 0.9 GB, Python and build trees the rest). The folders must have no space in their path: Zephyr's Kconfig step fails on one, and a symlink does not help. There is no `west flash` path for this board. Source: `am62l-evm/zephyr/README.md`.

| Image | Size | Fits | Start |
|---|---|---|---|
| `build-ddr` | 74.0 kB code, 5.4 kB data, 4.1 MB bss; `zephyr.bin` 77.5 kB | DDR at 0x82000000 | U-Boot `go 0x82000000` |
| `build-sram-all` | 124 of 144 kB | WKUP plus MAIN SRAM | JTAG |
| `build-sram-all-tight` | 120 of 144 kB | same | JTAG |
| `build-sram-main` | 44 kB over 80 kB | does not link | none |
| `build-ddr-core1` (E) | `zephyr.bin` 44.0 kB | DDR 0xE0000000 | PSCI from U-Boot |

## Play on the TV (game picker and gamepad)

The board boots into a list of games on HDMI and plays them with a USB or Bluetooth gamepad. No X, no container: `am62l-evm/lean/lean_fe.c` draws straight to the display and reads the pad from `/dev/input` (`docs/experiments/2026-11-01-am62l-lean-emulation.md`).

| Step | What to do |
|---|---|
| Pick | D-pad or left stick moves, L1/R1 page up and down, Cross, Circle or Options (A, B or Start on other pads) starts the game |
| Quit | Hold Select and Start together for 2 seconds: back to the list. On a DualShock 4 that is Share and Options |
| Pads that work | Sony DualShock 4 over USB (the one run on the bench, 2026-11-01). The code also maps Xbox, 8BitDo and other pads that report the standard gamepad buttons, and the common generic USB SNES pad layout; those were not tried. A second pad becomes player 2. Plug a pad in at any time, the list notices within a second |
| Mapping | Bottom button B, right A, left Y, top X, L1 L, R1 R, Share Select, Options Start. A DualShock 4 is not an SNES pad, so the face buttons follow position, not the printed symbols |
| Games | Put files in `/root/games` (any folder, two levels deep). By extension: `.sfc .smc` run Snes9x 2005, `.exe .cue .iso .pbp .chd` run PCSX-ReARMed (no BIOS file needed; PlayStation discs are untested), `.gb .gbc` run Gambatte. Copy one in with `scp mygame.sfc root@<board address>:/root/games/snes/`, then quit a game or run `systemctl restart frl-lean` and the list shows it. A line `file<TAB>Nice title` in `/root/games/titles.tsv` sets the name on the list. The free games that ship in the lab's picker and their licenses are in `am62l-evm/lean/games-sources.tsv` |
| Sound | Through the board's 3.5 mm headphone jack (the codec on the board), not HDMI. Volume: `amixer -c 0 sset PCM 90%` |
| Boot behaviour | `frl-lean.service` (`am62l-evm/lean/frl-lean.service`) starts the picker at boot and restarts it if it stops. It replaces the `ti-lvgl-demo` screen, which is disabled. Turn it off: `systemctl disable --now frl-lean`; get the old screen back: `systemctl enable --now ti-lvgl-demo` |
| Install on a fresh image | Build as in the experiment page, copy `lean_fe`, the cores and `frl-lean.service` into `/root/frl/lean` and `/etc/systemd/system`, then `systemctl enable --now frl-lean` |
| Logs | `journalctl -u frl-lean` shows the pad it found, the game started and the input latency |

## What was measured

Default Linux from a USB stick, both cores at 1.25 GHz (`performance` governor), 2026-10-22. Every row is in `data/results/am62l-evm.csv`.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| UART boot, ROM ping to U-Boot prompt | 420 s (409 to 423) | three files at 115200 baud, median of 3 | measured | `linux_boot_time`, `uart-rom-ping-to-uboot-prompt` |
| `booti` to login prompt | 21.4 s | median of 3 | measured | `linux_boot_time`, `booti-to-login-median3` |
| Kernel start to userspace | 3.9 s | `systemd-analyze`; 135 s of userspace, 120 s of it waiting for an unplugged second Ethernet port | measured | `linux_boot_time`, `systemd-kernel` |
| Power, Linux idle, eight INA228 rails | 561 mW | VDD_CORE 312, LPDDR4 91; one busy core adds 91 mW | measured | `rail_power_mw`, `total-linux-idle` |
| CoreMark, Linux, one core | 4,256 | GCC 15.3 -O2 | measured | `coremark`, `gcc15.3-linux-o2-1t` |
| CoreMark, Linux, one core, rechecked 2026-10-24 | 4,254 | governor `performance`, clock read at 1,246 MHz by a dependent-add loop; the 4,256 stands | measured | `coremark`, `gcc15.3-linux-o2-1t-recheck` |
| Linux to Zephyr message round trip, 56 bytes | 5.7 us | shared-memory rings, SGI and SPI doorbells (4.0 us polling) | measured | `ipc_roundtrip_us`, `zrpmsg-irq-56b` |
| CoreMark, Linux, two cores | 8,428 | GCC 15.3 -O2 | measured | `coremark`, `gcc15.3-linux-o2-2t` |
| CoreMark, Zephyr on core 1 | 4,007 | GCC 12.2 -O2, code in DDR, ring console | measured | `coremark`, `gcc12.2-zephyr-core1-o2-ddr` |
| Memory copy bandwidth, one thread | 1.6 GB/s | STREAM, processes in place of threads (libgomp missing) | measured | `mem_bandwidth`, `copy-1t` |
| DDR load latency | 271 ns | pointer chase, 64 MB, 4 kB pages | measured | `mem_latency`, `ddr-64m` |
| Scheduler jitter, maximum | 474 us idle, 546 us loaded | cyclictest | measured | `sched_jitter_us` |
| USB stick read and write | 38.1 and 10.4 MB/s | `dd`, direct I/O | measured | `storage_read_mbps`, `storage_write_mbps` |
| Ethernet transmit and receive | 927 and 900 Mbit/s | iperf3 to a host on the same switch | measured | `eth_throughput_mbps` |
| AES-128-CBC in software | 1.33 cycles per byte | OpenSSL, Armv8 crypto instructions, 16 kB blocks | measured | `aes_sw_cpb` |
| kmscube 1080p | 30 fps | llvmpipe; there is no GPU render node | measured | `gst_pipeline_fps`, `kmscube-1080p60-llvmpipe` |
| Sleep current, DDR self-refresh | not run | | not run | `sleep_current` |

The eight power monitors answer on `/dev/i2c-1` at 0x40 (VDD_CORE, 20 mOhm), 0x4C (3.3 V, 100 mOhm), 0x45 (1.8 V), 0x4D (VDDA 1.8 V), 0x47 and 0x49 (LPDDR4, 40 mOhm; only 0x49 carries load), 0x44 and 0x46 (RTC, 1 Ohm). No hwmon driver binds them; read registers 0x04 (shunt, 312.5 nV per LSB) and 0x05 (bus, 195.3125 uV per LSB) as 20-bit values in the top of 24 bits (`am62l-evm/tools/pwr.py`). HDMI: `modetest -M tidss` lists connector 44 (HDMI-A-1) with 11 EDID modes; `modetest -M tidss -s 44@42:1920x1080` sets the mode (stop `ti-lvgl-demo` first).

## If you see this

- If you see no output, or junk, on the serial port with the board powered: read with `capture.py`, which sets the rate and holds the port open. `stty -f` then `cat` reads at 9600 on macOS.
- If the console stays silent at the shipped switch setting: no SD card is in the slot and the boot ROM waits. Set UART boot (SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF) and unplug and replug J17.
- If DSLite prints `An invalid processor ID has been found`: the connection file lists only the DAP driver. Use `am62l-evm/tools/AM62L_TMDS62LEVM.ccxml.template`.

Every other error, with its cause tag: [FAQ](FAQ.md).

## Limits

Micro-SD boot depends on the card: one card gave `Transfer data timeout`, and a 119 GB card boots (the FAQ has the switches and seating). `build-ddr` and the ROM first-stage payload image build but have no run on the board.

## Where these facts come from

`data/results/am62l-evm.csv`, `am62l-evm/AGENTS.md`, `am62l-evm/FAQ.md`, `am62l-evm/zephyr/README.md`, `docs/experiments/2026-10-21-am62l-sram-zephyr.md`, `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`. Documents: user guide SPRUJG8B, data sheet SPRSPA1B, TRM SPRUJB4A.

Take only this board's files with a sparse clone:

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add am62l-evm tools data
```
