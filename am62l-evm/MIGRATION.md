# Moving coprocessor firmware to Zephyr on the second AM62L core

You can run the coprocessor job as a Zephyr image on A53 core 1 while Linux runs on core 0. Linux loads the image and starts the core itself, the two sides exchange RPMsg-framed messages through a 1 MB DDR window, and a 56-byte echo takes 4.0 to 5.7 us. This guide lists what that setup needs and what it costs, using only runs on the TMDS62LEVM (AM62L EVM). 1 kB = 1,024 bytes and 1 MB = 1,048,576 bytes in the numbers below.

Measured 2026-10-24 on TI Linux SDK 12.01.00.05.03 (kernel 6.18.38), upstream Zephyr at commit a915b2f, both cores at 1.25 GHz (performance governor). Source: `docs/experiments/2026-10-24-am62l-linux-zephyr-link.md`.

## What changes when the coprocessor becomes core 1

| Design point | Zephyr on A53 core 1 (measured here) |
|---|---|
| Who starts it | Your kernel module copies `zephyr.bin` to DDR and calls PSCI CPU_ON |
| Memory | One DDR range Linux never maps: `mem=1536M` leaves 0xE0000000 to 0xEFFFFFFF to Zephyr |
| Message transport | Two virtio-style rings with RPMsg framing, here a custom link, not OpenAMP and not remoteproc |
| Doorbell | SGI 7 to Zephyr, SPI 868 (INTID 900) to Linux, or polling |
| Cache rule | Window mapped Normal non-cacheable on both sides |
| Firmware image | `zephyr.bin`, 44.0 kB, linked at 0xE0000000, entry 0xE00008BC |

The name service, endpoints and the OpenAMP library are not implemented. If your Linux side talks through an existing RPMsg endpoint API, plan to port that layer (see [What is not shown](#what-is-not-shown)).

## Step 1: Boot Linux with core 1 left alone

Add this to the kernel command line (U-Boot `bootargs`):

```
maxcpus=1 mem=1536M cpuidle.off=1
```

`maxcpus=1` keeps Linux off core 1, `mem=1536M` keeps it out of the top 256 MB of the 2 GB DDR, and `cpuidle.off=1` matches the run (cpuidle on was not run). Check:

```bash
cat /sys/devices/system/cpu/online      # expect 0
```

## Step 2: Build the Zephyr image for core 1

The image is the link application `zephyr/apps/core1-rpmsg`, linked in DDR at 0xE0000000. On a build host that has run `zephyr/setup.sh` and the GIC patch step described in `zephyr/rebuild_core1.sh`:

```bash
bash am62l-evm/zephyr/rebuild_rpmsg.sh
```

It prints the size, the entry address and the sha256 of `zephyr.bin`. The entry address must match the one you pass in Step 4.

Two devicetree overlays make the memory usable:

- `core1-ddr.overlay` gives Zephyr 0xE0000000 to 0xEFFFFFFF as its only RAM.
- `core1-shm.overlay` adds a `zephyr,memory-region` node `FRL_SHM` at 0xEFF00000, size 0xFF000, with `zephyr,memory-attr = <0>`. Attribute 0 is Normal non-cacheable, so the other side sees each store without cache maintenance. The last page (0xEFFFF000) is the console ring, mapped the same way.

## Step 3: Build the Linux module on the board

The shared-window layout is in `rpmsg/zrpmsg_shm.h`, included by both the module (`rpmsg/zrpmsg.c`) and the Zephyr app. The board has gcc 15.3 but no kernel headers, so follow the [FAQ entry](FAQ.md#how-do-i-build-a-kernel-module-on-the-board): copy the SDK kernel source to the board, use the board's `/proc/config.gz` with a matching `CONFIG_LOCALVERSION`, run `modules_prepare`, then build with `rpmsg/Makefile`. MODVERSIONS is off, so only `vermagic` has to match `uname -r`.

## Step 4: Start Zephyr from Linux

```bash
insmod zrpmsg.ko
echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl
cat /sys/kernel/zrpmsg/result
```

The module writes the image to 0xE0000000, writes the control block, and issues PSCI CPU_ON for core 1. READY came back in under 5 ms. No U-Boot step is involved.

To stop and reload without a reboot:

```bash
echo stop > /sys/kernel/zrpmsg/ctl
echo "load /root/zrpmsg-zephyr.bin e00008bc" > /sys/kernel/zrpmsg/ctl
```

Zephyr ends itself with PSCI CPU_OFF when it receives the stop message. About 10 reloads ran in one boot.

## The window and the rings

| Item | Value |
|---|---|
| Window | 0xEFF00000 to 0xEFFFEFFF, 1 MB, outside Linux's memory |
| Mapping | Linux `memremap` write-combine; Zephyr `zephyr,memory-region` attr 0; both Normal non-cacheable |
| Rings | Two virtio split rings, 128 buffers each of 512 bytes |
| Direction | Ring 0: Linux to Zephyr. Ring 1: Zephyr to Linux |
| Roles | Linux is the driver side (fills buffers, available ring). Zephyr is the device side (returns buffers, used ring) |
| Message | 16-byte RPMsg header plus up to 496 bytes of payload |
| Control block | At the window base: magic, status (READY), wait mode, heartbeat, message count |
| Notification suppression | The virtio flags `ZR_F_NO_NOTIFY` and `ZR_F_NO_INTERRUPT` |

Porting your message structs: keep each message at or under 496 bytes of payload, or split it. Copies go through non-cacheable memory, which sets the latency below.

## The doorbells

- Linux to Zephyr: the module writes SGI 7 to `ICC_SGI1R_EL1`, targeting core 1. Zephyr handles it as an SGI.
- Zephyr to Linux: Zephyr writes `GICD_ISPENDR` (distributor at 0x01800000) to set SPI 868 pending. The module maps it with `irq_create_fwspec_mapping` and `request_irq`.
- Either side can poll the ring instead and skip its doorbell. Set the wait mode through the module (`mode <zmode> <lmode>`, 0 for interrupt, 1 for poll).

## What to expect

Echo round trip, median of three medians of 2,000:

| Payload | Both interrupt | Both poll |
|---|---|---|
| 56 bytes | 5.66 us | 4.03 us |
| 496 bytes | 18.38 us | 17.19 us |

Mixed modes land between: 4.40 us (Zephyr interrupt, Linux poll) and 5.09 us (Zephyr poll, Linux interrupt) at 56 bytes. At 496 bytes Zephyr's work is 15.0 us of the total, 62 reads and 62 writes of non-cacheable DDR.

A Linux thread that sleeps while it waits for the reply adds the scheduler wake: 12.7 us (56 bytes) and 25.2 us (496 bytes). A userspace `write` then `read` on `/dev/zrpmsg` takes 18.1 us (56 bytes) and 35.1 us (496 bytes). The 99th percentile runs 2 to 5 times the median.

Streaming, 20,000 messages:

| Direction | 496-byte messages/s | MB/s |
|---|---|---|
| Linux to Zephyr (interrupt or polling) | 668,000 | 316 |
| Zephyr to Linux, Linux by interrupt | 295,000 | 140 |
| Zephyr to Linux, Linux polling | 520,000 | 246 |

64-byte messages: 673,000 per second Linux to Zephyr, 299,000 Zephyr to Linux by interrupt. Every message was checked (first and last byte, sequence number): 0 bad. Run the benchmark yourself:

```bash
sh am62l-evm/rpmsg/bench.sh /root/bench.out
```

Use these numbers to size the job: control loops that fit in a round trip of 4 to 20 us and a few hundred thousand messages per second fit. A path that needs the userspace round trip at 18 to 35 us, or hard real-time bounds, needs its own measurement; interrupt latency under load was not measured.

Rows: `data/results/am62l-evm.csv`, `ipc_roundtrip_us`, `ipc_msgs_per_s` and `ipc_throughput_kbps` rows named `zrpmsg-*`.

## If you see this

- If Zephyr never takes the doorbell on a fresh build: the SGI number is 8 or higher. Only SGIs 0 to 7 can be enabled from the non-secure world; the enable write for 8 to 15 is ignored. Use SGI 7.
- If Zephyr starts but gets no doorbell after the first run: the earlier run ended with PSCI CPU_OFF inside the SGI handler, which leaves SGI 7 active and pending in core 1's redistributor (`ISACTIVER0` and `ISPENDR0` read 0x80). Stop from the main loop, never from the handler, and clear both bits at start.
- If every wait runs the full 200 ms: the module uses `wake_up_interruptible` on a `wait_event_timeout` queue. Use `wake_up`.
- If every reply is one message off: stale replies from an earlier command are still queued. Drain them before each command.
- If a reloaded image misbehaves: flush the image window (`dc civac`) before each reload. The run added this together with the fix above and did not isolate its separate effect.
- If you expect more compute from core 1: CoreMark is 4,254 on Linux (GCC 15.3 -O2, 3.40 per MHz at a verified 1.25 GHz) and 4,007 on Zephyr (GCC 12.2 -O2). Size the job against those.

## What is not shown

- Compatibility with the OpenAMP library or rpmsg-lite, the name service, endpoints, and remoteproc. The framing matches RPMsg; the rest is yours to port.
- A cacheable shared window. It needs core 1's cluster coherency checked and was not run.
- cpuidle on, both cores' interrupts under load, and Zephyr surviving a Linux reboot.
- Booting Zephyr as the boot ROM's first-stage payload. The ROM refused four image variants on this HS-FS part, so Linux has to start core 1 ([the Linux-to-Zephyr link experiment](../docs/experiments/2026-10-24-am62l-linux-zephyr-link.md)).
- Starting Zephyr from the device tree or an init script at boot. The runs loaded it by hand through sysfs after `insmod`.

Commands and the memory map for this board: [AGENTS.md](AGENTS.md#memory-map), [FAQ](FAQ.md).
