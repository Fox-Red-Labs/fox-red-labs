# AM62L family porting notes for AI coding assistants (TMDS62LEVM)

The AM62L is Linux-class, so the portable app runs as a Linux program and Zephyr runs on a second A53 core in DDR, that the image has no compiler or package manager, and that the board answers nothing on the serial port until the boot switches, the power cycle and the serial rate are all right. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [AM62L family FAQ](FAQ.md). **U-Boot** is the boot loader; **TF-A** is Arm's secure-world firmware; **PSCI** is the Arm call that starts and stops cores; **Jailhouse** is a partitioning hypervisor; **LSE** is the ARMv8.1 atomic instruction set. 1 kB = 1,024 bytes. Checked 2026-10-25 with TI Linux SDK 12.01.00.05.03 (kernel 6.18.38), Zephyr at commit a915b2f, CCStudio 21.0.1 and XDS110 firmware 3.0.0.32, on one TMDS62LEVM.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The part

| Fact | Value | Source |
|---|---|---|
| Chip | AM62L32: two Cortex-A53 up to 1.25 GHz, 160 kB on-chip SRAM; the debugger also lists SMS_CM4_0_TIFS | `am62l-evm/AGENTS.md` |
| Memory | 2 GB LPDDR4, 32 GB eMMC, micro-SD, 512 Mbit OSPI NOR at 0x60000000 | `am62l-evm/AGENTS.md` |
| Console | J7 FT4232HL; port A (`/dev/cu.usbserial-<id>0`) is UART0 at 115200 8N1 | `am62l-evm/AGENTS.md` |
| TI driver layers | none: no TI Drivers, no DriverLib; Linux sysfs, libgpiod and Zephyr drivers | `portability/three-ways/linux/README.md` |
| On the image | Docker 29.3.0, Python 3.14.5, Node 22.22.3, libgpiod tools 2.2.4; no compiler, no `apt`, no binfmt_misc, no KVM on the stock kernel | `am62l-evm/linux-apps/README.md`, `am62l-evm/AGENTS.md` |

## Start here

| Step | Command or file |
|---|---|
| Console | `python3 -I am62l-evm/tools/capture.py /dev/cu.usbserial-<id>0 115200 console.log` |
| Boot with no SD card | SW3.4 ON, SW3.3 ON, SW3.2 ON, SW3.1 OFF, full power cycle, `python3 -I am62l-evm/tools/uart_boot.py ...`, plug J17 (`am62l-evm/UART-BOOT.md`) |
| Run the portable app | copy `portability/three-ways/common/` and `linux/` to `/root/frl/tw`; `sh linux/build.sh /root/frl/tw`; `FRL_UART=/dev/ttyS0 timeout 14 /root/frl/tw/linux/build/three_ways_static` |
| Check it | `python3 portability/three-ways/serial_check.py /dev/cu.usbserial-<id>0 16 <log>` |
| Zephyr beside Linux | `am62l-evm/tools/psci_stub.py cpu_on 0x1 0xE00008BC` from U-Boot, Linux with `maxcpus=1 mem=1536M cpuidle.off=1` |
| Zephyr from Linux | `insmod zrpmsg.ko`, then `echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl` |
| Back up | nothing is flashed in these runs; the OSPI NOR was not written in any run |

## Rules for the assistant

1. **Hold the serial port open in the program that reads it.** `stty -f PORT 115200` then `cat PORT` reads at 9600 on macOS (`am62l-evm/tools/capture.py`).
2. **Do a full power cycle after any switch change.** SW7 does not re-read SW2, SW3 or SW4 (`am62l-evm/README.md`).
3. **Read the boot-mode table switch by switch.** The user guide lists SW3.4 first (`am62l-evm/UART-BOOT.md`).
4. **Build in a Debian trixie arm64 container on the board**, with `--network host`; install packages into a running container, because `docker build` and `docker commit` sat at 0 percent CPU for minutes (`am62l-evm/AGENTS.md`).
5. **Set `bootargs` before `booti`.** Without it the kernel prints `Starting kernel` and nothing else (`am62l-evm/AGENTS.md`).
6. **Put the stick in before power-up.** `usb start` after a hot-plug hangs U-Boot (`am62l-evm/AGENTS.md`).
7. **Clear `OPOST` on the tty in a program that writes lines**, and restore it with `stty -F /dev/ttyS0 opost` (`portability/three-ways/linux/README.md`).
8. **Free LD8 and SW5 with `am62l-evm/linux-apps/gpio_claim.sh free`** before driving them (`am62l-evm/linux-apps/README.md`).
9. **Do not put Zephyr in the on-chip SRAM beside Linux.** Link it in DDR at 0xE0000000, build it non-secure, and apply `am62l-evm/zephyr/patches/gic-skip-distributor.patch` (`am62l-evm/AGENTS.md`).
10. **Use the nogel DSLite board file.** CCStudio's stock `AM62L_SK_EVM` file runs a GEL that sets the PLL and initializes the DDR on connect (`am62l-evm/tools/load_sram.sh`).
11. **Keep the Zephyr workspace in a path without a space** (`am62l-evm/zephyr/README.md`).
12. **After `jailhouse disable` and a reboot, expect UART boot to stall**: unplug and replug J17; cause not found (`am62l-evm/AGENTS.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| Shared three-ways app | serial check passed; 66 glue lines | `portability/matrix.csv` X53 |
| Home Assistant, Node-RED, Pi-hole, Mosquitto containers | all ran; Home Assistant 557 MiB, 67.7 s | `am62l-evm/linux-apps/README.md` |
| gpiozero and Blinka | toggle LD8 after three workarounds | `am62l-evm/linux-apps/README.md` |
| Zephyr hello world in SRAM | printed on UART A over JTAG | `am62l-evm/README.md` |
| Zephyr on core 1 beside Linux | 56-byte echo 4.0 to 5.7 µs | `am62l-evm/MIGRATION.md` |

## Next

The family guide is `portability/families/am62l/README.md`; every port and test on record is `portability/families/am62l/EVIDENCE.md`; board bring-up is `am62l-evm/AGENTS.md`.
