# SK-TDA4VM as a Holoscan Sensor Bridge device

Terms: [vocab](../../docs/vocab.md), [SK-TDA4VM FAQ](../FAQ.md). **RoCE v2** is RDMA over Converged Ethernet, here carried in UDP packets; **PSN** is the packet sequence number.

The TI board plays the sensor side of NVIDIA's Holoscan Sensor Bridge: it enumerates, takes the host's register writes, and streams frames as RoCE v2 formatted UDP that a Holoscan host receives with NVIDIA's standard Linux receiver. The device code is NVIDIA's own HSB Emulator (Apache-2.0, `third-party/holoscan-sensor-bridge/`), built for the A72 cores with the files here. Proven on the board on 2026-10-10 over a loopback interface: 200 of 200 frames intact, 655 packets per 720p frame, zero sequence gaps. No real Holoscan host has received it (see Limits). Experiment page: `docs/experiments/2026-10-10-tda4-holoscan-sensor-bridge.md`.

## Files

- `Makefile`: builds `libhsbemu.a` from NVIDIA's `src/hololink/emulation` (common and Linux targets) and `serve_frames`. gcc and zlib only; 27 s on the board.
- `stub/cuda_runtime.h`: nine lines so the Linux data plane compiles without CUDA. It is only reached for GPU-resident buffers, which this board never has.
- `serve_frames.cpp`: the device program. `serve_frames <device_ip> <frames.raw> <frame_bytes> [--fps N] [--limit N]`. Frames are back-to-back raw images in a file; the host decides the pixel format.
- `hsb_host_check.py`: a host-side verifier written from NVIDIA's host code: enumerate, configure over ECB exactly as `DataChannel::configure_roce`, receive, reassemble, check PSN order, byte counts, frame numbers and frame CRCs against the source file. Python, so it holds about 10 fps at 720p; NVIDIA's receiver has not been tried.

## Run it on the board

```text
# build (board has gcc 13, make, zlib)
make -j2
# a test interface when there is no cable; with Ethernet use eth0's address instead
ip link add hsb0 type dummy; ip addr add 192.0.2.2/24 brd + dev hsb0; ip addr add 192.0.2.1/24 brd + dev hsb0; ip link set hsb0 up
# device
./serve_frames 192.0.2.2 frames720.raw 921600 --fps 10 &
# host-side check (on the board for loopback, or on any Linux box on the link)
sysctl -w net.core.rmem_max=134217728
python3 hsb_host_check.py --host-ip 192.0.2.1 --port 50000 --frame-bytes 921600 --frames 200 --source frames720.raw
```

Run it only on a private link between the board and one host: a commit security scan found the emulator's control-plane parser trusts the UDP length field (`third-party/holoscan-sensor-bridge/SOURCE.txt`). The emulator needs a broadcast address on its interface ("Broadcast address not provided" otherwise) and does not transmit until a host has configured it.

## Numbers (2026-10-10, loopback, `perf` task-clock of serve_frames)

| Stream | Nominal | MTU | A72 cost | RSS |
|---|---|---|---|---|
| 1920x1080 RAW8, 60 fps asked, loopback | 995 Mbit/s asked | 1500 | 98 percent of one core, saturated at about 42 fps (15.8 µs per packet on loopback) | 11 MB |
| 1280x720 RAW8 30 fps, real wire to a Mac | 226.7 Mbit/s, 300 of 300 frames intact | 1500 | 21.9 percent of one core (11.1 µs per packet) | 11 MB |
| 1920x1080 RAW8 30 fps asked, real wire | about 21.5 fps reached, 357 Mbit/s | 1500 | 37.4 percent of one core (about 11.8 µs per packet) | 11 MB |
| 1920x1080 RAW8 60 fps | 995 Mbit/s | 9000 | 37 percent of one core, 60 fps held (6.2 ms per frame, 3.8 times cheaper than MTU 1500) | 11 MB |
| 720p30, 720p60, 1080p30 | 221, 442, 498 Mbit/s | 1500 | about 31, 62, 70 percent (tick-accounted and scaled; see the experiment page) | 11 MB |

## Limits

The wire works to this repository's own checker. It has not been received by NVIDIA's `LinuxReceiverOperator`, which needs a Linux host on the network, and no camera feeds `DataPlane::send`; the frames come from a file. Only the A72 cores were used.
