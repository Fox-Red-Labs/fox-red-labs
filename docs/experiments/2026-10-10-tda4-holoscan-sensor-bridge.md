<!-- experiment
id: E151
board: SK-TDA4VM
date: 2026-10-10
status: in progress
reached: verified
line: NVIDIA's own Holoscan Sensor Bridge device emulator (Apache-2.0) builds on the SK-TDA4VM's Cortex-A72 Linux in 27 s with no CUDA and speaks the full Hololink protocol. Over a real 1 GbE link to this repo's checker on a Mac, 720p RAW8 frames arrived 100 of 100 intact at 10 fps and 300 of 300 at 30 fps, enumeration answered in 0.25 to 0.83 s. On the wire the sender costs 11.1 microseconds of one A72 per 1,408-byte packet (21.9 percent of a core at 720p30, receiver-confirmed rate); on loopback the same sender read 15.8 microseconds, suspected because loopback does the receive work inside the send call. 1080p at 30 fps over the wire averaged about 21.5 frames a second on 37 percent of a core and the Python checker lost packets there. Jumbo frames, tried only on loopback, held 1080p60 on 37 percent of a core. NVIDIA's own receiver did not see it.
stop: NVIDIA's receiver (needs a Linux host running NVIDIA's LinuxReceiverOperator; the wire itself now works with the repo's checker)
-->

# Can the SK-TDA4VM be a Holoscan Sensor Bridge: camera in, Hololink frames out over 1000BASE-T?

Terms used here are in [docs/vocab.md](../vocab.md). Code: [tda4/hsb/](../../tda4/hsb/), upstream source in [third-party/holoscan-sensor-bridge/](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/third-party/holoscan-sensor-bridge).

**Question.** A builder wants a camera into a TI processor and the frames sent to an NVIDIA Holoscan host over 1 Gbit Ethernet in the form Holoscan already receives, the Holoscan Sensor Bridge protocol ("Hololink"). Can the TDA4VM do the device side, on the Arm cores or on an R5F, and what does it cost?

**Vendor position.** NVIDIA, Holoscan Sensor Bridge documentation (read 2026-10-10): the device enumerates with BOOTP broadcasts once a second (device UDP port 12267, reply port 12268) carrying an "NVDA" vendor field; the host configures it through the Ethernet Control Bus, 14-byte register read and write requests on UDP 8192; frames go out as RoCE v2 RDMA WRITE packets (opcode 0x2A) ending in a WRITE WITH IMMEDIATE (0x2B) that carries 128 bytes of frame metadata. For hosts without a ConnectX NIC, "standard Linux sockets can be used to transmit and receive Ethernet frames that follow the RoCE packet structure ... enabling ... integration with custom FPGA or sensor platforms that generate RoCE-formatted packets." The repository ships an "HSB Emulator", "a minimally featured emulation of the HSB framework's IP for use in testing and development within a hosted or embedded environment", with Linux and STM32F767 (Cortex-M7) targets.

TI: no position. Nothing in TI's documentation mentions Holoscan or Hololink.

**What was tried.** Board: SK-TDA4VM, TI Edge AI SDK 11.00.00.08 (Linux 6.12, gcc 13.3, CMake 3.28, zlib, Python 3.12), no display, everything over the serial console; Ethernet to the home switch came later the same day.

1. Read NVIDIA's protocol documents and host source (`data_channel.cpp`, `enumerator.cpp`, `hololink.cpp`) and the emulator source (`src/hololink/emulation`).
2. Took the emulator's Linux target as is (`third-party/holoscan-sensor-bridge/`, commit 4b192ae). Its CMake enables CUDA unconditionally, but the only CUDA call is a device-to-host copy for GPU-resident buffers, so it was built with a plain Makefile and a 9-line stub header for that call (`tda4/hsb/Makefile`, `stub/cuda_runtime.h`). One build fix: NVIDIA's include order puts the platform headers first; this repo's headers had to match.
3. Wrote `serve_frames.cpp` (70 lines, after NVIDIA's `serve_linux_file.cpp`): one `HSBEmulator`, one RoCE v2 data plane, frames from a raw file at a set rate.
4. Wrote `hsb_host_check.py`, a host-side verifier that does what NVIDIA's host library does: listen for the BOOTP broadcast and decode the vendor field, configure the data plane over ECB with the same register writes as `DataChannel::configure_roce` (packet size in 128-byte pages, frame length, host MAC, IP and port, QP, R_Key, buffer address, page increment, buffer count, sensor enable mask), then receive on the configured port, reassemble by RETH virtual address, and check PSN order, bytes written, frame numbers and each frame's CRC-32 against the source file.
5. Ran both on the board over a dummy network interface (192.0.2.2 device, 192.0.2.1 host, broadcast address set), since the bench had no cable.
6. With a cable in, ran the same sender on `eth0` and the checker on a Mac on the same switch, at 720p 10 and 30 fps and 1080p 30 fps, with `perf stat` on the sender.

**How far it got.** Measured. Build 27 s. Enumeration, control plane and data plane all worked at the first try once the interface had a broadcast address.

| Step | Result |
|---|---|
| Enumeration | BOOTP request seen 0.55 to 0.91 s after the checker started: 300 bytes, vendor tag 0xE0, "NVDA", Ethernet port 0, enumeration version 2, UUID and serial present, HSB IP version 0x2602 |
| Control plane | 18 ECB transactions (one read of the version register, 15 writes, two read-modify-writes of the enable mask), every reply status 0 |
| Data plane, 1280x720 RAW8 (921.6 kB) at 10 fps | 200 of 200 frames intact against the source CRCs, 655 packets per frame (921.6 kB in 1.4 kB payloads), 0 PSN gaps, 75.5 Mbit/s |
| Data plane, 1920x1080 RAW8 (2.07 MB) | 3 of 3 frames intact, 1,473 packets per frame |
| Metadata packet | opcode 0x2B, immediate = page and PSN, bytes written 921.6 kB, frame number counting up, QP and R_Key as configured. Fields arrive in network byte order (the FPGA document says little-endian; the emulator uses `htonl`) |
| MTU 9000 | 3 of 3 frames intact at 235 packets per frame; the sender held 60 fps and its cost per frame fell 3.8 times |
| Receiver limit | At 30 fps the Python checker (78 percent of a core) overran the default socket buffer and dropped 11 percent of packets, so frames assembled corrupt; raising `net.core.rmem_max` and running at 10 fps fixed it. The kernel's receive-buffer-error counter stopped climbing. This is the checker's limit, not the device's; NVIDIA's receiver is C++ |

Device-side cost, `serve_frames` process alone, `perf stat` task-clock over 10 s after a host configured it (the first pass used `/proc` tick accounting, which undercounts this sender 1.9 times because it wakes on the timer tick; those rows stay in the CSV marked as such):

| Stream | Nominal rate | MTU | A72 cost | Per packet | RSS |
|---|---|---|---|---|---|
| 1920x1080 RAW8, 60 fps asked, about 42 fps reached | 995 Mbit/s asked | 1500 (1,473 packets per frame) | 98 percent of one core, saturated | 15.8 µs, 31.7 thousand cycles, 24.6 thousand instructions per packet; 23.3 ms of A72 per frame | 11 MB |
| 1920x1080 RAW8, 60 fps held | 995 Mbit/s | 9000 (235 packets per frame) | 37 percent of one core | 26.4 µs per packet; 6.2 ms of A72 per frame, 3.8 times cheaper per frame | 11 MB |
| other rates (tick-accounted, scaled by 1.9) | | 1500 | about 31 percent at 720p30, 62 at 720p60, 70 at 1080p30 | | |

How the rates were read: the sender prints a line every 300 frames. In the first perf run its lines 17.2 s and 24.3 s after start, inside the perf window, are 300 frames apart, about 42 fps at MTU 1500; in a rerun at MTU 9000 the lines at 8.0 s and 13.0 s are 300 frames apart, 60 fps; a repeat run reproduced both readings within 1 percent. An earlier version of this page divided by the 60 fps asked for and gave 11.1 µs per packet and 2.65 times; the sender never reached 60 at MTU 1500. The cost is per packet, flat across rates: the payload copy, the RoCE headers, a software CRC-32 over the packet and one `sendto`. Jumbo frames are the first lever; the Holoscan host guide already sets MTU 4200 for RoCE.

Caveat: on loopback the kernel also receives every packet, so system-wide load was higher (about half of both cores at 1080p60, most of it softirq).

**On the wire (2026-10-10, later).** With the board on the home network at 1000 Mb/s and the checker on a Mac on the same switch (macOS caps its socket buffer at 8 MB, so the checker ran with 7 MB), MTU 1500 on the whole path:

| Stream | Frames intact | Rate the receiver saw | Sender cost, perf over 8 s | Per packet |
|---|---|---|---|---|
| 1280x720 RAW8, 10 fps | 100 of 100 | 10.1 fps, 76.0 Mbit/s | not measured | |
| 1280x720 RAW8, 30 fps | 300 of 300 | 30.1 fps, 226.7 Mbit/s | 21.9 percent of one A72 | 11.1 µs, 22.2 thousand cycles |
| 1920x1080 RAW8, 30 fps | 5 (the checker lost packets: 68,404 sequence gaps) | about 21.5 frames a second of data, 357 Mbit/s | 37.4 percent of one A72 | about 11.8 µs at that rate |

So on a real link the sender costs about 11 µs per packet, and the 15.8 µs loopback figure above carries extra work (suspected: loopback delivers each packet to the receiver inside the sender's send call). Why the 1080p stream ran below 30 fps over the wire without saturating a core is unknown (suspected: the checker's lost packets, the switch or the send buffer). Jumbo frames could not be tried on this path: the home switch and the Mac run MTU 1500.

**The line.** No line reached on the device side: the protocol works over a real 1 GbE link at 720p30 with the repo's checker. The stop is this: NVIDIA's `LinuxReceiverOperator` on a Linux host taking the stream. That needs a Linux PC with the `hololink` Python package (no GPU is needed for the receive path); the cable is now on the bench.

R5F: not run. NVIDIA's emulator has a Cortex-M7 target with lwIP, so the code is MCU-shaped; a TDA4VM R5F port means TI's RTOS SDK with lwIP on the CPSW and a camera-to-R5F buffer path. Written up as phase two in the study.

- Jumbo frames (the host guide uses MTU 4200 for RoCE): about 4 percent less overhead and a third as many packets.
- `SEPARATE_FRAMEMETADATA_PACKET=1` and the CoE (IEEE 1722) data plane, which NVIDIA added for hosts without RDMA.
- A C++ receiver on the board for a higher-rate loopback check.
- PTP: the metadata carries timestamps; the emulator fills them from the system clock.

**Ask.** A reader with a Holoscan host: run NVIDIA's `linux_imx274_player.py`-style receiver against this board's stream and report fps and drops. And report the camera format, since 1 GbE caps at about 1080p RAW10 at 30 fps.
