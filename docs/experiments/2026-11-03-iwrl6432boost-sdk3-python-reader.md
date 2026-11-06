<!-- experiment
id: E300
board: IWRL6432BOOST
date: 2026-11-03
status: works
reached: verified
line: The most-forked Python reader for mmWave SDK 3 point clouds (ibaiGorordo/AWR1843-Read-Data-Python-MMWAVE-SDK-3-) reads every point of two IWRL6432BOOST walk recordings, identical to the recordings, with its parser unedited; it needs NumPy 1.x (NumPy 2 stops it at the first point), and on macOS it reads nothing live until the port is wrapped so a read comes before the bytes-waiting count
stop: none for recorded streams; live points not yet checked with a person in front of the radar
-->
# Does the popular Python mmWave SDK 3 reader work on the IWRL6432BOOST?

Terms: [vocab](../vocab.md), [IWRL6432BOOST FAQ](../../iwrl6432boost/FAQ.md). **TLV** is a type-length-value block in a radar frame.

**Short answer.** Yes, with the board set to send standard points (`guiMonitor 1 ...`) and NumPy 1.26. The script's own parser read 537 frames and 3,489 points from two real walk recordings with no difference from the recordings. Live on macOS it needs a small port wrapper; with it, it decoded 120 frame headers in 30 s, the board's 4 frames a second.

**Question.** `ibaiGorordo/AWR1843-Read-Data-Python-MMWAVE-SDK-3-` (176 stars, 43 forks, read 2026-11-03) and its SDK 2 twin are the readers most hobby and student radar projects start from. Does its parser read the IWRL6432 low-power radar's stream?

**What we tried.**

1. Set the board to the standard point format: `guiMonitor 1 0 0 0 0 1 1 0 1 0 0 0` instead of our `guiMonitor 2 ...`. The firmware then sends TLV 1 (x, y, z, velocity as float32) and TLV 7 (SNR, noise), the layout the script reads (MMWAVE-L-SDK 05.05.04.02, `motion_detect.c`).
2. Re-encoded the two walks recorded on 2026-11-03 (one person each, 328 frames) into that exact stream with `ports/radar-host/common/reencode_walk.py`. The points are the recorded ones.
3. Ran `readAndParseData18xx()` from `readData_AWR1843.py` at `66f16c2`, unedited, through `ports/radar-host/sdk3-python-reader/run.py`. The script runs its own main loop at import (fixed COM ports, a Qt plot), so the harness loads the file up to its MAIN line and hands the parser a port object. The replay releases one frame per 250 ms and polls every 50 ms, as the script's loop does.
4. Ran it live on the board's data port for 30 s, with nobody in front of the radar.

**What stopped it, in order.**
- **NumPy 2.** With NumPy 2.5.4 it stops at the first point: `ValueError: setting an array element with a sequence` (line 213). The script assigns a one-element array to an array slot, which NumPy 2 refuses. With NumPy 1.26.4 it runs.
- **macOS serial.** On this Mac the board's XDS110 data port reports 0 bytes waiting until a read is issued. The script only calls `read(in_waiting)`, so it reads nothing: 0 frames in 30 s. `run.py --mac-glue` wraps the port so `in_waiting` reads first and reports what it holds; then it decoded 120 headers in 30 s.

**How far it got.** Run. Every point was checked on the recordings. Live, the frames were checked but not the points, because the room was empty.

| Run | Frames with points | Points | Difference from the recording | Row |
|---|---|---|---|---|
| Walk v1, NumPy 1.26.4 | 265 of 265 | 1,696 of 1,696 | 0 | `radar_host_points_delivered`, `sdk3-python-reader-walk-v1` |
| Walk v2, NumPy 1.26.4 | 272 of 272 | 1,793 of 1,793 | 0 | `sdk3-python-reader-walk-v2` |
| Walk v1, NumPy 2.5.4 | stops at the first point | | | `sdk3-python-reader-numpy2` |
| Live, 30 s, no glue / with glue | 0 / 120 headers | 0 (empty room) | | note on the walk-v2 row |

Records: `ports/radar-host/sdk3-python-reader/logs/run-2026-11-03.log`. No expectation was written before the runs.

**The line.** None for recorded streams. Live points need a person moving in front of the radar.

**What to try next.**
- Run it live with someone walking, on `guiMonitor 1`, and compare against the board's own parser on the same frames.
- Everything here applies to `kirkster96/IWR6843-Read-Data-Python-MMWAVE-SDK` (100 stars), which reads the same layout.
