# TI TMDS62LEVM (AM62L EVM): notes for an AI coding assistant

That the board answers nothing on the serial port until the boot switches, the power cycle and the serial rate are all right, and that the on-chip SRAM is not free once Linux or TF-A runs. This file is the short list; each item comes from runs on one board.

Errors and fixes: the [AM62L EVM FAQ](FAQ.md). Terms: **TF-A** is Arm's secure-world firmware; **TIFS** is TI's security firmware for the chip; **PSCI** is the Arm call that starts and stops CPU cores; **SCMI** is the mailbox Linux uses to request clocks and power; **GEL** is a debugger startup script; **DSLite** is TI's command-line debug server. 1 kB = 1,024 bytes. Checked 2026-10-21 and 2026-10-22 on an Apple M4 Mac with CCStudio 21.0.1 (DSLite 21.0.0.4166), XDS110 firmware 3.0.0.32, TI Linux SDK 12.01.00.05.03 (kernel 6.18.38) and Zephyr at commit a915b2f, on one TMDS62LEVM.

**How to use it.** Copy this file into the top folder of your project as `AGENTS.md`. Claude Code reads `CLAUDE.md`: copy it under that name, or add the line `@AGENTS.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence lives; links only work there.

## The board

| Part | Fact |
|---|---|
| Chip | AM62L32: two Cortex-A53 up to 1.25 GHz, 160 kB on-chip SRAM |
| Memory | 2 GB LPDDR4, 32 GB eMMC, micro-SD (MMC1), 512 Mbit OSPI NOR at 0x60000000 |
| Power | USB-C PD into J17; the source must offer more than 15 W. LD6 board power, LD5 PD good for J17 |
| Console | J7 is an FT4232HL: port A SoC UART0 (console), B UART1, C WKUP UART0, D UART4. 115200 8N1. macOS names `/dev/cu.usbserial-<id>0` to `3` |
| Debugger | J8 on-board XDS110. DSLite lists CortexA53_0, CortexA53_1, SMS_CM4_0_TIFS (`docs/experiments/2026-10-21-am62l-sram-zephyr.md`) |
| LEDs, buttons | LD8 user LED on GPIO0_123 (Zephyr `led0`); SW7 warm reset; SW5 user interrupt on GPIO0_90 |

## Start here

| Step | This board |
|---|---|
| Console | `python3 -I am62l-evm/tools/capture.py /dev/cu.usbserial-<id>0 115200 console.log` (`am62l-evm/tools/capture.py`) |
| Boot with no SD card | SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF, full power cycle, then `python3 -I am62l-evm/tools/uart_boot.py /dev/cu.usbserial-<id>0 --tiboot3 tiboot3.bin --tispl tispl.bin --uboot u-boot.img --log uart-boot.log` and plug J17 |
| Linux | SDK `.wic.xz` written to a 32 GB USB stick with `xz -dc IMAGE \| sudo dd of=/dev/rdiskN bs=4m`; at `=>`: `usb start`, load kernel and dtb from `usb 0:2`, `booti` with `root=/dev/sda2` |
| Zephyr in SRAM | `am62l-evm/tools/load_sram.sh --list-cores`, then `--listen --core 0`; switches on no boot (`am62l-evm/tools/load_sram.sh`) |
| Zephyr beside Linux | `am62l-evm/tools/psci_stub.py cpu_on 0x1 0xE00008BC` from U-Boot, then Linux with `maxcpus=1 mem=1536M cpuidle.off=1` (`docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`) |
| Build Zephyr | `am62l-evm/zephyr/setup.sh` (3 GB, path without a space); `rebuild_sram.sh`; `ONLY=el1 rebuild_core1.sh` (`am62l-evm/zephyr/README.md`) |
| Jailhouse | From U-Boot (files from TI's Jailhouse image on the stick): `load usb 0:2 ${loadaddr} /boot/Image-jailhouse`, `load usb 0:2 ${fdtaddr} /boot/dtb-jailhouse/k3-am62l3-evm.dtb`, `setenv bootargs console=ttyS0,115200n8 earlycon=ns16550a,mmio32,0x02800000 root=/dev/sda2 rootwait rw cma=64M`, `booti ${loadaddr} - ${fdtaddr}`. In Linux: `modprobe jailhouse`, `jailhouse enable /usr/share/jailhouse/cells/k3-am62l3-evm.cell` (0.27 s, prints `Activating hypervisor`), `jailhouse cell create /usr/share/jailhouse/cells/k3-am62l3-evm-inmate-demo.cell`, `jailhouse cell load inmate-demo /usr/share/jailhouse/inmates/uart-demo.bin`, `jailhouse cell start inmate-demo`; FT4232 port B (UART1, 115200) prints `Hello 1 from cell!` and on. `gic-demo.bin` prints `Timer fired, jitter: N ns`. Stop with `jailhouse cell shutdown inmate-demo`, `jailhouse cell destroy inmate-demo`, `jailhouse disable` (`docs/experiments/2026-10-22-am62l-jailhouse.md`) |

## Boot switches

SW3 = BOOTMODE[15:12]; ON is 1; the user guide lists SW3.4 first. Full power cycle after any change; SW7 does not re-read them (`docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`).

| Mode | SW3.4 | SW3.3 | SW3.2 | SW3.1 |
|---|---|---|---|---|
| As shipped (SD primary, full pincount; SW4 = ON ON OFF OFF OFF OFF ON OFF, SW2 = OFF ON OFF OFF) | OFF | OFF | OFF | OFF |
| SD primary, UART backup | ON | OFF | ON | OFF |
| UART primary, SD backup | ON | ON | ON | OFF |
| No boot for JTAG: SW4.4 to SW4.7 ON, SW3 all OFF | OFF | OFF | OFF | OFF |

## Traps

| Trap | Fix | Source |
|---|---|---|
| `stty -f PORT 115200` then `cat PORT` reads at 9600 on macOS; a 115200 line looks silent | Hold the port open in the program that reads: `capture.py`, `uart_boot.py`, `ub.py`, `sh.py` | `am62l-evm/tools/capture.py` |
| CCStudio's stock `AM62L_SK_EVM` board file runs a GEL on connect that sets the PLL and initializes the DDR | Use `AM62L_TMDS62LEVM_nogel_board.xml` and the ccxml `load_sram.sh` generates | `am62l-evm/tools/load_sram.sh` |
| DSLite: `An invalid processor ID has been found` | The ccxml lists only the DAP driver; add `cortexA53`, `cortexM`, `etbcs`, `csstm`, `ctset2` (the template does) | `am62l-evm/tools/AM62L_TMDS62LEVM.ccxml.template` |
| DSLite cannot connect to core 1 while it is powered down (error -1062) | Start core 1 with PSCI `CPU_ON` from U-Boot (`psci_stub.py`) | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| DSLite warns it wants XDS110 firmware 3.0.0.43; the board has 3.0.0.32 | Ignore; it connects and loads | `docs/experiments/2026-10-21-am62l-sram-zephyr.md` |
| Switches change but nothing new happens | Unplug and replug J17; SW7 does not re-read them | board run, 2026-10-21 and 22 |
| Boot-mode table read in reverse | The user guide lists SW3.4 first; read each switch number on the silkscreen | board run, 2026-10-21 and 22 |
| `usb start` after hot-plugging a stick into a running board hangs U-Boot | Put the stick in before power-up | board run, 2026-10-21 and 22 |
| Micro-SD: `Transfer data timeout`, then `Card did not respond to voltage select! : -110` | Bad or unseated card; the eMMC still answers. Use UART boot plus a USB stick | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| SRAM images beside Linux | Do not. Linux owns 0x70800000 to 0x7080FFFF and TF-A code sits at 0x707F0000 (map below) | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| TF-A starts a secondary core at non-secure EL1, not EL3 or EL2 | Build the image non-secure (`core1-ns.conf`); no EL3 code path | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| After `jailhouse disable` and `reboot`, UART boot sends `tiboot3.bin` but SPL never pings for `tispl.bin` (330 s, three tries) | Full power cycle (unplug and replug J17); cause not found | `docs/experiments/2026-10-22-am62l-jailhouse.md` |
| `booti` without `setenv bootargs` | The kernel prints `Starting kernel` and nothing else; only a reset recovers it. Set `bootargs` first (`printenv bootargs`) | `am62l-evm/README.md` |
| The board's DHCP address changes at every boot | Read it from the console: `am62l-evm/tools/sh.py "ip -4 -o addr show eth1"` | `am62l-evm/README.md` |
| A UART boot sent more than 5 minutes after power gets `receiver cancelled` or `block not acknowledged` | Power cycle, then send within the ROM's window; a `reboot` from default Linux returns to the ROM ping and works | `am62l-evm/README.md` |
| Zephyr 144 kB SRAM image panicked in the simulator with 6 translation tables | Use 8 (`bare-el3.conf`, 32 kB); the 80 kB map does not link, floor 104 kB | `am62l-evm/zephyr/README.md` |
| Zephyr builds fail in a folder with a space in its path | Move the workspace; a symlink does not help | `am62l-evm/zephyr/README.md` |
| Linux and Zephyr both want the GIC distributor | Build Zephyr with `patches/gic-skip-distributor.patch` so it touches only its own core's registers | `am62l-evm/zephyr/README.md` |
| `/dev/mem` read of a region Linux uses | `mem=1536M` keeps Linux out of 0xE0000000 and up; read the ring there | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |
| `cpuidle.off=1` was set beside Zephyr | Not tested without it | `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md` |

## Memory map

| Address | Size | Owner | Note |
|---|---|---|---|
| 0x02800000 | | UART0 (16550, 4-byte register stride) | Console |
| 0x60000000 | 512 Mbit | OSPI NOR, memory mapped | Not written in any run |
| 0x707F0000 to 0x707FFFFF | 64 kB | WKUP SRAM | TF-A resume stub, 34 kB of live code under Linux; free on a bare board |
| 0x70800000 to 0x7080FFFF | 64 kB | MAIN SRAM `oc_sram`, claimed by Linux | First 0x100 bytes are the SCMI mailbox |
| 0x70810000 to 0x70813FFF | 16 kB | MAIN SRAM, firewalled | Bus error from the normal world |
| 0x80c00000 | | U-Boot `fdtaddr` | `am62lx.env` |
| 0x82000000 | | `build-ddr` load and `go` address | |
| 0x88000000 | | Default stub address in `psci_stub.py` | Free DDR while U-Boot idles |
| 0xE0000000 to 0xEFFFFFFF | 256 MB | Image E (entry 0xE00008BC) and its ring | Outside Linux with `mem=1536M` |
| 0xEFFFF000 | 4 kB | Console ring | u32 magic 0x52524C46, u32 head (bytes written), u32 size 4080, u32 reserved, then data |

Sources: `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`, `am62l-evm/tools/ringcat.py`.

## Commands that worked

```
python3 -I am62l-evm/tools/uart_boot.py --selftest                 # no board needed
am62l-evm/tools/load_sram.sh --list-cores
am62l-evm/tools/load_sram.sh --listen --core 0
am62l-evm/tools/load_sram.sh --peek 0x040841B4,2                   # read-only reads after a run
python3 am62l-evm/tools/psci_stub.py version                       # rc 0x10001 (PSCI 1.1)
python3 am62l-evm/tools/psci_stub.py cpu_on 0x1 0xE00008BC         # rc 0x0
python3 am62l-evm/tools/psci_stub.py affinity 0x1                  # 1 = off, 0 = on
python3 -I am62l-evm/tools/ringcat.py --addr 0xEFFFF000 --once     # on the board, as root; /dev/mem
python3 am62l-evm/tools/sh.py "ip -4 -o addr show eth1"           # console shell as root; DHCP address
python3 am62l-evm/tools/pwr.py 30 (copy to the board)           # on the board: eight INA228 rails, 30 s mean (README, battery section)
modetest -M tidss -s 44@42:1920x1080                               # on the board, after systemctl stop ti-lvgl-demo
```

Sources: `am62l-evm/tools/load_sram.sh`, `psci_stub.py`, `ringcat.py`, `uart_boot.py`. `ringcat.py` defaults to the SRAM ring at 0x70813000; the run read 0xEFFFF000 through `/dev/mem`.

## Status

Works: UART boot to U-Boot (about 7 minutes); Linux from a USB stick; Zephyr hello in SRAM over JTAG; Zephyr on core 1 beside Linux (DDR); micro-SD boot (119 GB card, 2026-10-24); Jailhouse (TI's flow, 2026-10-22). Should work (built, not run on the board): `build-ddr`, ROM first-stage payload. Not for this chip: Zephyr in SRAM beside Linux, the 80 kB map.

## Linux to Zephyr link (2026-10-24)

- `echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl` after `insmod zrpmsg.ko` starts Zephyr on core 1 from Linux (boot Linux with `maxcpus=1 mem=1536M cpuidle.off=1`); `stop` ends it cleanly; `sh am62l-evm/rpmsg/bench.sh OUT` runs the benchmark. Source: `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md`.
- Doorbell SGI must be 0 to 7; stop Zephyr from its main loop; flush the image window before a reload. Source: same page.
- CoreMark 4,254 to 4,256 at a verified 1.25 GHz (3.40 per MHz, GCC 15.3 -O2); do not expect 5 per MHz. Source: `data/results/am62l-evm.csv`, `gcc15.3-linux-o2-1t-recheck`.
- The ROM refuses `mkphase1.sh` images on this HS-FS part (four variants); that case is still open. Source: same page.

## HMI, Doom and controller (2026-10-24)

- No GPU: LVGL 9.3 fixed 1080p scene 54.1 fps on fbdev (29.8 percent of both cores, 2.0 MB), 18.0 fps on its DRM backend (45.8 percent), Slint 1.17.1 software renderer 41.9 fps (46.7 percent, 24.6 MB); median of 3, 30 s each. Source: `docs/experiments/2026-10-24-am62l-hmi-shootout.md`.
- Chocolate Doom 3.1.0 (shareware v1.9, Debian arm64 container): timedemo demo1 6.54 fps on Weston with pixman, about 6 fps on SDL `kmsdrm`; Mesa llvmpipe runs the scaler. Source: `docs/experiments/2026-10-24-am62l-doom-controller.md`.
- Wired PS4 pad: `/dev/input/event1`, stick up reads a low `ABS_Y` (3 at full up, 120 centered).
- SD boot stall cause and fix: `docs/experiments/2026-10-24-am62l-sd-boot-stall.md` (SW4 not at the as-shipped value and the card not seated).
- Containers: `docker` works on the SDK image (Debian trixie arm64 pulled over Ethernet); `apt` does not exist on the host. Source: `am62l-evm/hmi/run_hmi.sh` runs.

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

## Edge AI, jitter and SD card (2026-10-24)

- CPU-only on 2 A53 at 1.25 GHz: whisper.cpp tiny.en 2.23x real time, base.en 4.75x (11 s clip, 2 threads); Qwen2.5-0.5B Q4_0 2.89 tokens/s generation, 477 MB; YAMNet 20.2 ms, Silero en_v5 868 ms (2 threads). Source: `docs/experiments/2026-10-24-am62l-edge-ai-cpu.md`.
- cyclictest worst case on the stock PREEMPT kernel: 442 us idle, 1,074 us loaded (median of 3 and 5 runs), one unexplained 93,699 us. Source: `docs/experiments/2026-10-24-am62l-realtime-jitter.md`.
- Root works over ssh at the DHCP address; `pip install` of cmake, ninja, onnxruntime and ai-edge-litert works on the stick root. Kernel lacks HID_PLAYSTATION, HID_SONY, UINPUT; an on-board build of hid-playstation loads. A new 128 GB card shows as `mmcblk1` and holds the SDK 12.01 default image (SD boot needs SW3 all OFF). Source: `docs/experiments/2026-10-24-am62l-sd-card-and-ps5-driver.md`.

## SD boot stall (2026-10-24)

- SW3 all OFF with the 119 GB card and the card not fully seated: console shows ROM `C`, BL1 notices, `ENTERING WFI - end of bl1`, then silence, repeating; no SPL, no Linux. Cause: the card was half in (see the FAQ); seated, `mmc rescan` then `boot` reached the login. Source: `docs/experiments/2026-10-24-am62l-sd-boot-stall.md`.

## Linux apps (2026-10-24)

- TI's SDK 12.01 image already has Docker 29.3.0, Python 3.14, Node 22 and libgpiod tools; there is no apt, and `opkg update` cannot reach its package feed outside TI's network. Add software with `docker run` or `pip3 install --break-system-packages`. Source: `am62l-evm/linux-apps/README.md`.
- Mosquitto 2.1.2 runs as a service on 127.0.0.1:1883; `systemctl stop mosquitto` before publishing a container on that port.
- Measured (one run each): Mosquitto container 2.6 MiB, Node-RED 5.0.9 110 MiB (17.7 s to the editor), Pi-hole 2026.10.0 24.8 MiB, Home Assistant 2026.10.4 557 MiB (67.7 s to the onboarding API, 3.42 GB image, 655 s to pull); rails stay within 5 mW of 568 mW. Source: `data/results/am62l-evm.csv`, tests `linux_app_*`.
- LD8 (GPIO0_123) and SW5 (GPIO0_90) are on gpiochip0, owned by `leds-gpio` and `gpio-keys`; `am62l-evm/linux-apps/gpio_claim.sh free` releases them. Reading LD8 back returns 0; check the output register with `gpio_regs.py`.
- gpiozero needs a board description (`gpiozero_demo.py`), lgpio needs swig 4.3.1 and a source-built C library, Blinka needs `blinka_am62l.py`.
- SD boot: with SW3 all OFF, SW4 and SW2 as shipped and J17 unplugged for 30 s, the 119 GB card booted Linux on 2026-10-24 (root `/dev/root` 116 GB); the earlier BL1 stall did not recur on that start.

## Portable app, Doom on the framebuffer, Steam, Moonlight (2026-10-25)

- `portability/three-ways/common/app.c` runs unchanged as a Linux program: ADC from IIO sysfs, line to `/dev/ttyS0`, LED class sysfs; 66 glue lines, 5 config lines, 67,680-byte stripped binary, 1.2 s build; serial check PASS. Clear `OPOST` on the tty or lines end in `\r\r\n`. Source: `portability/three-ways/linux/README.md`.
- Doom straight into `/dev/fb0` in software: 186 to 193 fps timedemo at 1280x1000 (scale 4x5), 28 times the llvmpipe path; stop `ti-lvgl-demo` first. The picture reads as 4:3 on the HDMI monitor, checked by eye. Source: `ports/doom/linux-fb/README.md`.
- No compiler on the image: build in a Debian trixie arm64 container on the board (`--network host`). Here `docker build` and `docker commit` sat at 0 percent CPU for minutes; install packages into a running container instead. Source: `am62l-evm/steam/setup.sh`.
- The kernel has no binfmt_misc, no KVM (`CONFIG_KVM` not set, CPUs start at EL2) and no Android binder.
- Steam under Box64 0.3.4 installs (2.4 GB) and stops before any window. Source: `docs/experiments/2026-10-25-am62l-steam-box64.md`.
- Valve's native arm64 Steam client (public beta) draws Sign in to Steam 20 to 43 s after start with `lse_trap.so` (the A53 lacks ARMv8.1 LSE atomics; plain run is `SIGILL`), `avshim.so` on FFmpeg 8 (Ubuntu 26.04 container), no `taskset 0x7c` in `steamwebhelper.sh`, `--shm-size=512m` and `lsof`; 1.2 to 1.3 GB system RAM in use. Source: `docs/experiments/2026-10-25-am62l-steam-box64.md` (Second push).
- Any arm64 binary that dies with `Illegal instruction` here: check the `dmesg` address for an LSE instruction (`echo 1 > /proc/sys/debug/exception-trace`) and preload `am62l-evm/steam/lse_trap/lse_trap.so`. The kernel runs armhf programs (`CONFIG_COMPAT=y`).
- moonlight-embedded f32e415 builds on the board in 282 s with FFmpeg software decode and SDL2; not run (no stream host). Source: `am62l-evm/moonlight/build.sh`.
- Steam third push: non-root (uid 1000) with Chromium's sandbox on needs `--security-opt seccomp=unconfined` on that container only; bridge network works. `LSE_PATCH=1` hot-patches LSE sites (`lse_trap/lse_patch.h`): drawn sign-in page 46 to 49 s (56 to 57 trapped), idle traps under 10 per s; idle page costs about half a core in software rendering, 38 percent of one core with `-cef-disable-gpu -cef-disable-gpu-compositing`. Empty `/dev/shm` between starts or the next start crashes.
- Custom kernel `6.18.38-ti-kvm` runs from the SD card: `kvm [1]: Hyp nVHE mode initialized successfully`, `/dev/kvm`, binderfs (`binder`, `hwbinder`, `vndbinder`), binfmt_misc, `hid-playstation`. U-Boot loads `/boot/Image` and `/boot/dtb/ti/k3-am62l3-evm.dtb` from the ext4 root (`bootpart=1:2`); stop autoboot with `am62l-evm/tools/ub_reboot.py` and hand-boot the stock `Image-6.18.38-ti-00934-g8933766be737-dirty` to recover (proven). KVM guest CoreMark 95 to 98 percent of the host (QEMU `-machine virt,gic-version=2 -accel kvm -cpu host -nic none`); Redroid 12 boot completed in 86 s. Source: `docs/experiments/2026-10-25-am62l-kvm-binder-kernel.md`.
- Open games: SuperTuxKart 1.5 arm64 4.5 fps at 640x480, 2.4 at 1280x720 on llvmpipe; its x86_64 build segfaults under Box64 0.3.4; dsda-doom 27.2 / 10.5 fps; anonymous Steam downloads work natively with DepotDownloader (5.6 MB/s). Source: `docs/experiments/2026-10-25-am62l-open-games.md`.
- Windows on Arm: Windows 11 Pro 22H2 build 22621.1 arm64 installs as a KVM guest (`-machine virt,gic-version=2 -accel kvm -cpu host -smp 2 -m 1600`, AAVMF, ramfb, NVMe, swtpm, a 2 GB swap file on the host): WinPE at 80 s, first restart at 5,073 s, OOBE at 6,302 s, then stops on "Why did my PC restart?" (wants a network for an update). Build the ISO with `efisys_noprompt.bin`; eject it at the first restart. Source: `docs/experiments/2026-10-25-am62l-windows-arm-vm.md`.
- HDMI demo loop: `am62l-evm/showcase/run.sh` loops Doom on `/dev/fb0`, `ti-lvgl-demo`, Redroid 12 through scrcpy 5.0.1 (built on the board, 1.1 frames a second shown), SuperTuxKart 1.5 at a 640x480 HDMI mode (2 fps, race 7 min after launch) and RetroArch Gambatte (48 fps at 640x480, 10.4 at 1080p), in the `frl-show` container (Xorg `modesetting`, `AccelMethod none`). Doom and LVGL need X stopped (DRM master). Source: `docs/experiments/2026-10-25-am62l-hdmi-showcase.md`.

## Lean emulation on HDMI (2026-11-01)

- Emulators draw straight to DRM/KMS with no X and no container: `am62l-evm/lean/lean_fe.c` (libretro frontend). Snes9x 2005 60 fps at 640x480 to 1920x1080, core 2.5 to 3.6 ms of 16.7 ms, free homebrew; PCSX-ReARMed 60 fps on a PlayStation GPU test program, 3.6 ms. Source: `docs/experiments/2026-11-01-am62l-lean-emulation.md`.
- The showcase's 48 fps and 10.4 fps Game Boy numbers came from Xorg, the container and llvmpipe scaling: the same Gambatte core runs 60 fps at 640x480 and 1080p through DRM. Source: same page.
- The tidss plane has no scaler: `drmModeSetPlane` is accepted 1:1 and returns EINVAL for any larger destination, so scale integer ratios on the CPU into a dumb buffer (0.35 ms at 640x480, 1.3 to 1.9 ms at 1080p). Source: same page.
- Snes9x 2010 `tile.c` takes over 14 minutes and 800 MB with GCC 15.3 at `-O2` on the board; use Snes9x 2005. Source: same page.
