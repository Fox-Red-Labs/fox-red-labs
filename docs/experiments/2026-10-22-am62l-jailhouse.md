<!-- experiment
id: E212
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-22
status: works
reached: verified
line: TI's Jailhouse flow from SDK 12.01.00.05.03 ran as documented on the AM62L EVM, booted from a USB stick: the hypervisor enabled in 0.27 s, the uart-demo and gic-demo inmates printed on UART1 (FT4232 port B) within a second of the cell start, and a second Linux (6.18.38, one core, RAM filesystem) reached a login on UART1 about 20 s after its cell start. Stop, destroy and disable returned 0 (disable 0.09 s). The run shows no Zephyr cell, no inmate-to-root message traffic and no timing beyond the figures here. Zephyr has Jailhouse support upstream on other boards; nothing was built for this one.
stop: none
-->
# Does TI's Jailhouse hypervisor run on the AM62L EVM, and how fast do its demo cells start?

**Short answer.** Yes, as TI documents it. With the Jailhouse kernel and device tree from the SDK image on a USB stick, `jailhouse enable` took 0.27 s, the bare-metal `uart-demo` and `gic-demo` printed on UART1 at once, and the Linux inmate booted to a login on UART1 about 20 s after `cell start`. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md). 1 KB = 1,024 bytes.

**Question.** TI ships a Jailhouse image for the TMDS62LEVM. Does its documented demo sequence run, what does each stage cost in time, and does the second core run an inmate while Linux stays up on the first?

**Vendor position.** TI documents the flow in the Linux SDK 12.01.00.05.03 Hypervisor page, Jailhouse: https://software-dl.ti.com/processor-sdk-linux/esd/AM62LX/12_01_00_05_03/exports/docs/linux/Foundational_Components/Hypervisor/Jailhouse.html . This run follows its commands. TI's page boots from an SD card; this run booted from a USB stick (below). The page names no Zephyr or FreeRTOS inmate.

**What was tried.** 2026-10-22 from 14:32 to 14:46 on one board, console on UART A, inmate output on UART1 (FT4232 port B), both at 115200.

- Files: from `tisdk-jailhouse-image-am62lxx-evm-12.01.00.05.03.rootfs.wic.xz` (sha256 26f62e139ac72b644be93ef3fe9d42173cfa4152f861dba81547483d370ce5e0) the Jailhouse kernel (`Image-jailhouse`, version 6.18.38-ti-g7bb6651c0bbf-dirty), its device tree (`dtb-jailhouse/k3-am62l3-evm.dtb`, with a 1 GB no-map reservation `jailhouse@c0000000` at 0xC0000000), the inmate cpio (142 MB), `/usr/sbin/jailhouse`, `/usr/share/jailhouse`, the kernel module directory and `/usr/lib/firmware/jailhouse.bin` were copied under new names onto the USB stick that already held the default image. The image's root partition does not fit the stick, so only these were copied. The image's own `tiboot3.bin`, `tispl.bin` and `u-boot.img` differ from the default image's (TI's page says its U-Boot carries a fix for the reserved memory) and were sent over UART.
- Boot: `reboot` from Linux returned the ROM to UART boot with SW3 as set for UART boot; `am62l-evm/tools/uart_boot.py` sent the three files (U-Boot reached 415.6 s after the ROM ping), then at `=>`:

```
usb start
load usb 0:2 ${loadaddr} /boot/Image-jailhouse
load usb 0:2 ${fdtaddr} /boot/dtb-jailhouse/k3-am62l3-evm.dtb
setenv bootargs console=ttyS0,115200n8 earlycon=ns16550a,mmio32,0x02800000 root=/dev/sda2 rootwait rw cma=64M
booti ${loadaddr} - ${fdtaddr}
```

  TI's page gives no kernel arguments and lists no overlay; `cma=64M` is the value in the image's `uEnv.txt`. No `fdt apply` was used. Kernel start to login prompt: 22.7 s. The kernel reserved `jailhouse@c0000000` (1,048,576 KiB) and reported 2 GB RAM.
- Demo sequence, commands from TI's page, run as root on UART A:

```
modprobe jailhouse
jailhouse enable /usr/share/jailhouse/cells/k3-am62l3-evm.cell
jailhouse cell create /usr/share/jailhouse/cells/k3-am62l3-evm-inmate-demo.cell
jailhouse cell load inmate-demo /usr/share/jailhouse/inmates/uart-demo.bin
jailhouse cell start inmate-demo
jailhouse cell shutdown inmate-demo
jailhouse cell destroy inmate-demo
(create, load gic-demo.bin and start the same way)
jailhouse cell create /usr/share/jailhouse/cells/k3-am62l3-evm-linux-demo.cell
jailhouse cell load k3-am62l3-evm-linux-demo /usr/libexec/jailhouse/linux-loader.bin -a 0x0 -s "kernel=0xc0800000 dtb=0xc0600000" -a 0x1000 /boot/Image-jailhouse -a 0xc0800000 /boot/tisdk-jailhouse-inmate-am62lxx-evm.cpio -a 0xc314a000 /usr/share/jailhouse/inmate-k3-am62l3-evm.dtb -a 0xc0600000
jailhouse cell start k3-am62l3-evm-linux-demo
jailhouse cell shutdown k3-am62l3-evm-linux-demo
jailhouse cell destroy k3-am62l3-evm-linux-demo
jailhouse disable
```

  The Linux load line differs from TI's only in the kernel file name (`/boot/Image-jailhouse` for TI's `/boot/Image`).

**How far it got.** Configure, load, run and verified for each stage.

| Stage | Result | Time |
|---|---|---|
| `modprobe jailhouse` | rc 0 | not timed |
| `jailhouse enable` | hypervisor v0.12 on CPU 1 and CPU 0, SMMU, PVU and PCI units, root cell `k3-am62l3-evm` running on CPUs 0 and 1, UART A kept working | 0.27 s |
| `uart-demo` | `Hello N from cell!` on UART1 with no pad or clock setup beyond TI's cell and device tree; 7,852 lines in the first 15 s (the line rate of 115200 baud) | first line within 1 s of `cell start` |
| `gic-demo` | `Initializing the GIC...`, `Initializing the timer...`, then one `Timer fired` line about every 105 ms; 359 samples: jitter 764 ns minimum, 16,919 ns maximum, 1,499 ns mean | first line within 1 s of `cell start` |
| Linux inmate | kernel 6.18.38 boots from the 142 MB cpio on UART1 (ttyS3), login banner, `root` logs in, `nproc` 1; the root cell sees CPU 0 only while it runs | `cell load` 5.07 s; kernel first line within 1 s of `cell start`; login banner about 20 s after |
| shutdown, destroy, `jailhouse disable` | rc 0; CPU 1 returned to the root cell (`online` 0-1) | disable 0.09 s |

While a cell was created or destroyed the root cell logged `scmi_protocol scmi_dev.1: Timeout waiting for a free TX channel !` once each time, and `disable` logged an `OF: ERROR: memory leak, expected refcount 1 instead of 5` for the virtual PCI node. Nothing stopped and no later command failed; no cause was investigated. The timings are the host clock at the moment the command was sent against the host-stamped UART1 capture, good to about one second except `enable`, `load` and `disable`, which are `time` figures from the board.

**After the run.** A `reboot` from the Jailhouse kernel (hypervisor disabled and the module removed first) put the ROM back in UART boot and the ROM pinged, but three sends of `tiboot3.bin` (the image's own once, the default image's twice) were each followed by 330 s without the ping from SPL for `tispl.bin`; the same sender had passed twice that day, including once from a warm `reboot`. The board was left in that state and needs a full power cycle (J17), after which the default kernel boots with its usual U-Boot commands. Cause not found.

**The line.** No stop on TI's documented flow. What the run does not show: inmate-to-root traffic (`ivshmem-demo`, the shared-memory network, the crash test), a Zephyr or FreeRTOS cell, interrupt latency under load, power, a cell started after Linux has run for hours, or the micro-SD boot TI's page assumes (the card on hand failed earlier the same day). The expectations written before the run were mostly cautious: UART1 was expected to stay silent until its pads and clock were set; it printed with the SDK's configuration unchanged.

**Zephyr as an inmate (research only, no build).** Upstream Zephyr has Jailhouse pieces: a `jailhouse_debug_console` driver (`drivers/console/jailhouse_debug_console.c`), Jailhouse mentions in the board docs of the NXP i.MX93 EVK, NXP LS1046A RDB and MediaTek Genio 510 and 700 EVKs, and an ivshmem service (`doc/services/virtualization/ivshmem.rst`). The Jailhouse tree and NXP's fork carry Zephyr cell files for other boards (`configs/arm64/imx8mp-zephyr.c`, `imx8mm-zephyr.c`, `qemu-arm64-zephyr-demo.c`). None is for the AM62L. The earlier Zephyr image (45.1 kB, linked at 0xE0000000, ring console at 0xEFFFF000, EL1) lies inside TI's 1 GB reserved window (0xC0000000 to 0xFFFFFFFF); a cell would need the AM62L cell file's memory region for it, a CPU 1 assignment, and either UART1 (the inmate-demo cell already grants it) or the ring. That is port work and was not run.

**Ask.** If you know why the root cell logs the SCMI `Timeout waiting for a free TX channel` on each cell create and destroy (the inmate's CPU is stopped and restarted through PSCI), say where it is documented.
