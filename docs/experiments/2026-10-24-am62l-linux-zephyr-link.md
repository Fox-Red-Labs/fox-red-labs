<!-- experiment
id: E250
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: works
reached: verified
line: A shared-memory message link between Linux on A53 core 0 and Zephyr on core 1 ran on the AM62L EVM, with Linux starting Zephyr itself (PSCI CPU_ON from a kernel module, no U-Boot step). Two virtio-style rings with RPMsg framing sit in DDR outside Linux's memory; Linux rings Zephyr with SGI 7, Zephyr rings Linux by setting SPI 868 pending. Echo round trip, median of three medians, 56-byte payload: 5.7 us by interrupt both ways, 4.0 us polling both ways; 496-byte payload 18.4 and 17.2 us; 18.1 us from a userspace write and read. Streaming 20,000 messages: Linux to Zephyr 667,000 messages/s (331 MB/s at 496 bytes), Zephyr to Linux 295,000 messages/s by interrupt (146 MB/s) and 520,000 by polling (258 MB/s). Zero bad messages in every run. It is not the OpenAMP library and not remoteproc. A separate try at Zephyr as the boot ROM payload was refused by the ROM and did not run.
stop: Zephyr as the ROM payload, a different question: the ROM acknowledged a 54-block transfer of a self-made certificate plus Zephyr four times and asked for a new image each time.
-->
# How fast can Linux on one AM62L core talk to Zephyr on the other?

**Short answer.** A message round trip takes 5.7 us with a doorbell interrupt in both directions and 4.0 us with both sides polling (56-byte payload, median of three runs of 2,000). A full 496-byte message takes 18.4 us, almost all of it Zephyr copying 496 bytes through non-cacheable DDR. Streaming 496-byte messages runs at 331 MB/s from Linux to Zephyr and 146 MB/s back (258 MB/s back when Linux polls). Rates here use 1 kB = 1,000 bytes. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**Question.** With Linux on core 0 and Zephyr on core 1 and no hypervisor ([Linux and Zephyr on separate cores](2026-10-22-am62l-linux-zephyr-amp.md)), can the two exchange RPMsg-style messages through shared DDR with a hardware doorbell each way, and what do latency and throughput look like?

**What was built.** `am62l-evm/rpmsg/zrpmsg_shm.h` (layout), `zrpmsg.c` (Linux kernel module), `am62l-evm/zephyr/apps/core1-rpmsg` (Zephyr), `rebuild_rpmsg.sh`, `bench.sh`, `rows.py`.
- Window: 0xEFF00000 to 0xEFFFEFFF, outside Linux's memory (`mem=1536M`), mapped Normal non-cacheable on both sides (Linux `memremap` write-combine, Zephyr a `zephyr,memory-region` node, attr 0).
- Two virtio split rings of 128 buffers of 512 bytes; each message is a 16-byte RPMsg header plus up to 496 bytes. Linux is the driver side, Zephyr the device side. Notification suppression follows the virtio flags.
- Linux to Zephyr doorbell: SGI 7 written to `ICC_SGI1R_EL1` from the module. Zephyr to Linux: a write of `GICD_ISPENDR` for SPI 868 (INTID 900), which the module maps with `irq_create_fwspec_mapping` and `request_irq`. Either side can poll instead.
- Linux loads `zephyr.bin` (44.0 kB, sha256 45e426f1838d89f9d51aa5ac2006e5eb3152b9d03c76d40f24e2edc92d77f004, entry 0xE00008BC) to 0xE0000000 and calls PSCI CPU_ON for core 1. READY came back in under 5 ms. Zephyr stops itself with PSCI CPU_OFF on a STOP message, so images can be reloaded without a reboot (about 10 reloads in the run).
- The module was built on the board with its own gcc 15.3 against the SDK kernel source (`modules_prepare` with the board's `/proc/config.gz`; no MODVERSIONS, so only `vermagic` has to match). Steps are in the board FAQ.

**Results** (2026-10-24, kernel 6.18.38, both cores 1.25 GHz performance governor, one core online; shared system counter at 200 MHz, so 5 ns steps; every row is in `data/results/am62l-evm.csv`).

| Echo round trip, payload | Both by interrupt | Zephyr interrupt, Linux poll | Zephyr poll, Linux interrupt | Both poll |
|---|---|---|---|---|
| 56 bytes | 5.66 us | 4.40 us | 5.09 us | 4.03 us |
| 496 bytes | 18.38 us | 17.58 us | 17.95 us | 17.19 us |

Breakdown, both by interrupt, 56 bytes: doorbell to Zephyr 1.48 us, Zephyr work 2.39 us, answer to the Linux handler 1.75 us. Polling cuts the two hops to 1.06 and 0.56 us. At 496 bytes Zephyr's work is 15.0 us: 62 reads and 62 writes of non-cacheable DDR (the 271 ns load latency in the README). A sleeping kernel thread that waits for the reply adds the scheduler wake: 12.7 us (56 bytes) and 25.2 us (496 bytes) in total. From a userspace `write` then `read` on `/dev/zrpmsg`: 18.1 us (56 bytes) and 35.1 us (496 bytes). p99 values run 2 to 5 times the median (a few microseconds of Zephyr timer ticks and Linux interrupts).

| Streaming, 20,000 messages | Payload | Messages/s | kB/s |
|---|---|---|---|
| Linux to Zephyr (same with Zephyr polling) | 64 bytes | 673,000 | 43,100 |
| Linux to Zephyr | 496 bytes | 668,000 | 331,000 |
| Zephyr to Linux, Linux by interrupt | 64 / 496 bytes | 299,000 / 295,000 | 19,100 / 146,500 |
| Zephyr to Linux, Linux polling | 64 / 496 bytes | 533,000 / 520,000 | 34,100 / 257,700 |

Linux to Zephyr costs about 1.5 us a message whatever the size, because neither side copies the payload. Zephyr to Linux by interrupt is slower because Zephyr rings the doorbell for every message (a device-memory write plus the Linux handler); polling removes that. Pooled medians of 6 runs per row; every payload's first and last byte and sequence number checked, 0 bad.

**Findings that cost time (each is a line in the board FAQ).**
1. Only SGIs 0 to 7 can be enabled from the non-secure world. A write to `GICR_ISENABLER0` of core 1's redistributor from Linux set bits 0 to 7 and ignored bit 8 to 15. The first build used SGI 8 and never got an interrupt.
2. Ending Zephyr with PSCI CPU_OFF inside the SGI handler left SGI 7 active and pending in core 1's redistributor (read through `/dev/mem`: `ISACTIVER0` 0x80, `ISPENDR0` 0x80). The next start then never received a doorbell. Fix: stop from the main loop, and clear the active and pending bits at start. A cache clean and invalidate of the image window (`dc civac`) before each reload was added in the same step as a precaution; its separate effect was not isolated.
3. `wake_up_interruptible` does not wake `wait_event_timeout` (uninterruptible): every wait ran the full 200 ms. Use `wake_up`.
4. Queued messages from an earlier command made every later reply one off; the module now drains stale replies before each command.

**What this does not show.** OpenAMP or rpmsg-lite compatibility (the framing matches RPMsg, the name service and endpoints are not implemented); remoteproc; a cacheable shared window (needs core 1's cluster coherency checked, not run); cpuidle on (it was off); both cores' interrupts under load; Zephyr surviving a Linux reboot.

**Side check 1: CoreMark recheck.** The 4,256 on one core was not a clock problem: governor `performance`, `scaling_cur_freq` 1,250,000 kHz, and a dependent-add loop measured 1,246.1 to 1,246.5 MHz. The same binary gave 4,254.4. So 3.40 CoreMark per MHz is what GCC 15.3 -O2 produces on this A53, and the expectation of 6,000 to 7,000 was wrong.

**Side check 2: Zephyr as the ROM's phase-1 payload.** Expectation, written before: about 25 percent chance of a banner. Result: no. With the board in UART boot, the ROM accepted the full 54-block XMODEM transfer of `mkphase1.sh` output (certificate plus the 53,392-byte `build-sram-all` image) and returned to its `C` ping about 3 s later, for four variants: default (extended boot info, random 4096-bit key, load 0x707F0000), `--legacy`, `--degenerate`, and both. No Zephyr output on UART A. The SoC is AM62LX SR1.1 HS-FS (U-Boot banner). TI's own `tiboot3.bin` carries four components (SBL at 0x70800000, TIFS, board configuration, TIFS inner certificate); the self-made one had one. A fifth attempt that re-signed TI's own `tiboot3.bin` with the self-made key stopped being acknowledged part-way and was not pursued further; the ROM then needed about 4 minutes to start pinging again. The follow-on runs (the same image from SD card and OSPI NOR) were not run because they need this to work.

**Ask.** Whether the AM62L boot ROM on an HS-FS part accepts any image not signed with the customer key, and what the minimum component list is.
