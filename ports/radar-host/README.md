# Which open radar programs read the IWRL6432BOOST's stream?

Two of the three programs tried read it once the board sends the standard point format: the Python reader as it is, the ROS driver when frames come less than 100 ms apart. Set `guiMonitor 1 0 0 0 0 1 1 0 1 0 0 0` in the cfg; the firmware then sends TLV 1 (x, y, z, velocity as float32) and TLV 7 (SNR, noise) under the mmWave SDK 3 header.

Terms: [vocab](../../docs/vocab.md). TLV is a type-length-value block in a radar frame.

| Program | Result | Folder | Page |
|---|---|---|---|
| radar-lab/ti_mmwave_rospkg (ROS 1) | Every point, identical, at frames 50 ms apart; none at 250 ms | `ti-mmwave-rospkg/` | E299 |
| ibaiGorordo/AWR1843-Read-Data-Python-MMWAVE-SDK-3- | Every point of two walk recordings, identical; NumPy 1.x; macOS port wrapper | `sdk3-python-reader/` | E300 |
| vilari-mickopf/mmwave-gesture-recognition | Reads no frame: SDK 2 point layout, its own firmware's length rule | `gesture-recognition/` | E301 |

`common/reencode_walk.py` turns a recorded walk (`iwrl6432boost/live/runs/*-walk-*.json`) into the exact stream the firmware sends with `guiMonitor 1`, so each program can be checked point by point. `common/walk-v1-tlv1.bin` and `walk-v2-tlv1.bin` are those streams.

The third-party code is not copied here. Each folder's script takes a clone of the repo at the commit named in its log.
