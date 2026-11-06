<!-- experiment
id: E299
board: IWRL6432BOOST
date: 2026-11-03
status: line found
reached: run
line: The reference ROS 1 driver for TI mmWave (radar-lab/ti_mmwave_rospkg) builds on ROS Noetic with one build-file edit and publishes every point of an IWRL6432BOOST walk recording, identical to the recording, when frames arrive every 50 ms; at the board's 250 ms frame period it publishes nothing, because its serial reader pushes a stale byte on each 100 ms read timeout and every frame then fails its length check
stop: frame period over 100 ms; the driver's read loop, or a cfg with frames under 100 ms
-->
# Does the reference ROS driver for TI mmWave read the IWRL6432BOOST?

Terms: [vocab](../vocab.md), [IWRL6432BOOST FAQ](../../iwrl6432boost/FAQ.md). **ROS** is the Robot Operating System; a **driver node** turns the sensor's serial stream into ROS messages. **TLV** is a type-length-value block in a radar frame.

**Short answer.** Yes, once two things change. The driver builds on ROS Noetic after one edit to its build file (C++11 to C++14). Its startup parameters have to be set by hand, because they normally come from an xWR-style cfg. Given frames 50 ms apart, it then published all 265 frames and all 1,696 points of a real walk recording, identical to the recording within 2.4e-7 m. At the board's 250 ms frame period it published nothing.

**Question.** `radar-lab/ti_mmwave_rospkg` is the most-used open driver for TI mmWave sensors (315 stars, 107 forks, read 2026-11-03). It was written for the IWR1443, 1642, 1843 and 6843. Does it read the IWRL6432 low-power radar?

**What the board sends.** With `guiMonitor 1 ...` in place of our `guiMonitor 2 ...`, the firmware sends TLV 1 (x, y, z, velocity as float32) and TLV 7 (SNR, noise) under the 40-byte mmWave SDK 3 header (MMWAVE-L-SDK 05.05.04.02, `motion_detect.c`). That is the layout the driver parses for SDK 3. The header's version byte is 5, so the driver takes its SDK 3 path, and the platform field (0xA6432) gives it the 8-word header.

**What we tried.**

1. Re-encoded the walk recorded on 2026-11-03 (one person, 328 frames, 1,696 points, spoken cues) into the exact byte stream the firmware sends with `guiMonitor 1`, using `ports/radar-host/common/reencode_walk.py`. The points are the recorded ones. Noise, which the recorder does not keep, is 0.
2. Built the driver at `cb3255f` in `ros:noetic-ros-base` (arm64, Docker on a Mac) with `ports/radar-host/ti-mmwave-rospkg/in_container.sh`. The build stops with `#error PCL requires C++14 or above` until the two C++11 lines in `CMakeLists.txt` say C++14. No source file was edited.
3. Started the driver on a virtual serial port (`socat`) and fed the stream into it with `feed.py`, one frame per period. `record_points.py` stored every `PointCloud2` on `/ti_mmwave/radar_scan_pcl`, and `compare.py` checked them against the recording, after the driver's axis swap (ROS x = radar y, ROS y = minus radar x).

**What stopped it, in order.**

- **Startup parameters.** The driver's constructor loops until `/ti_mmwave/doppler_vel_resolution` exists (`DataHandlerClass.cpp`, line 11). Its config node sets that by parsing `profileCfg` and `frameCfg`, which a low-power SDK cfg does not have, so the driver spins forever and publishes nothing. Fix: set the eleven parameters from our cfg before starting it (`in_container.sh`). On the SDK 3 path only the frame time is used, as the marker lifetime.
- **Frame period.** The serial reader reads one byte at a time with a 100 ms timeout and pushes the byte even when the read times out (`DataHandlerClass.cpp`, `readIncomingData`). Any gap between frames longer than 100 ms adds stale bytes, and the frame then fails `totalPacketLen == currentBufp->size()` and is dropped.

**How far it got.** Run, on a re-encoded recording; not on the live board, because Docker on macOS can't pass the USB serial port into the container.

| Frame period | Frames published | Points published | Row |
|---|---|---|---|
| 250 ms (our cfg) | 0 of 265 | 0 of 1,696 | `radar_host_points_delivered`, `ti-mmwave-rospkg-frame-250ms` |
| 120 ms | 0 of 265 | 0 | `...-frame-120ms` |
| 90 ms | 213 of 265 | 1,358 (80.1%) | `...-frame-90ms` |
| 50 ms | 265 of 265 | 1,696 (100%), largest difference 2.4e-7 m | `...-frame-50ms` |

At 90 ms the feed's timing jitter in Docker sometimes crosses the 100 ms timeout. Records: `ports/radar-host/ti-mmwave-rospkg/logs/run-2026-11-03.log`, rows in `data/results/iwrl6432boost.csv`. No expectation was written before the runs.

**The line.** The driver works on this board's points when frames arrive less than 100 ms apart. Our people-counting cfg runs at 250 ms.

**What to try next.**
- Run the board at a frame period under 100 ms. `frameCfg 2 0 200 64 250 0` has 12.8 ms of chirps per frame, so the chirps fit, but TI's 100 ms tracking cfg hung the shipped 05.04 image after about 31 frames (`iwrl6432boost/live/README.md`). Check a faster cfg for that hang first; the tracker and presence settings were also tuned for 250 ms.
- Or change the read loop to push a byte only when `read()` returns one (two lines). That is an edit to the driver, so it belongs upstream.
- Run it live on a Linux host with the board on USB, where the serial port reaches ROS directly.
- The ROS 2 path (`kimsooyoung/mmwave_ti_ros`, 76 stars) is next in the list. Its read loop has not been checked.

