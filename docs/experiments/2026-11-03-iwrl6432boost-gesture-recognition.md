<!-- experiment
id: E301
board: IWRL6432BOOST
date: 2026-11-03
status: line found
reached: run
line: The most-starred open gesture recognizer for TI mmWave (vilari-mickopf/mmwave-gesture-recognition) reads none of an IWRL6432BOOST stream: its config loader stops on a low-power SDK cfg, and its frame parser, built for its own AWR1642 firmware's SDK 2 layout, rejects every frame on its length check; its models also want hand gestures within 1 m at 10 frames a second, so running it needs a format converter and new gesture recordings
stop: frame format (SDK 2 point layout and length rule) and training data (near-field gestures at 10 frames a second)
-->
# Can the popular mmWave gesture recognizer run on the IWRL6432BOOST?

Terms: [vocab](../vocab.md), [IWRL6432BOOST FAQ](../../iwrl6432boost/FAQ.md). **TLV** is a type-length-value block in a radar frame. **Q-format** stores a fraction in an integer with a fixed number of fraction bits.

**Short answer.** Not as it is. Its config loader stops on our cfg, and its parser reads 0 of 328 frames, because it expects the frame layout of the AWR1642 firmware build it ships. Its models were trained on hand gestures within 1 m at 10 frames a second, in 25-frame windows. Running it means converting our points to its layout and recording new gestures by hand.

**Question.** `vilari-mickopf/mmwave-gesture-recognition` (167 stars, 27 forks, last push 2024-09) trains CNN, ResNet, LSTM and Transformer models on nine gestures (up, down, left, right, clockwise, counterclockwise, Z, S, X) from an AWR1642's serial output. Can the IWRL6432 low-power radar feed it?

**What it expects** (read from its source at `07a1ec8`):
- A cfg in the xWR16 command set (`profileCfg`, `frameCfg`, `guiMonitor` with seven values). It computes range bins and maximum velocity from them (`formats.py`).
- TLV 1 in the SDK 2 layout: a count and a Q-format word, then per point six uint16 values (range index, Doppler index, peak, x, y, z). Our firmware sends SDK 3 float points instead.
- 10 frames a second (`frameCfg ... 100` in its profile). The model takes 25 frames of up to 40 points each.
- Points within 1 m of the sensor. `Polar(max_distance=1.)` drops everything farther.

**What we tried.**

1. Loaded the repo's `formats.py` and `parser.py` unedited with `ports/radar-host/gesture-recognition/try_parser.py`. Its package `__init__` files import TensorFlow 2.15, scikit-learn and Qt, which the loader and parser don't use. Installing the full requirements left SciPy and NumPy mismatched, so the harness loads the two files directly.
2. Gave its loader our people-counting cfg, then its own `profile.cfg`.
3. Gave its parser the walk recorded on 2026-11-03, re-encoded as the firmware's TLV 1 + TLV 7 stream (`ports/radar-host/common/reencode_walk.py`).

**How far it got.** Run, to the parser.

| Step | Result | Row |
|---|---|---|
| Loader, our cfg | `IndexError: list index out of range` (a low-power SDK command has fewer values than its xWR16 namesake) | |
| Loader, the repo's `profile.cfg` | OK: maximum velocity 2.353 m/s, 512 range bins | |
| Parser, 328 frames | 0 frames read, `Corrupted frame` on each | `radar_host_points_delivered`, `gesture-recognition-parser` |

The parser removes the 40-byte header, then checks the stated packet length (160 bytes) against the bytes left (120). It is written for the length its shipped AWR1642 firmware build reports. Records: `ports/radar-host/gesture-recognition/logs/run-2026-11-03.log`. No expectation was written before the runs.

**The line.** Frame format, then training data. A converter can fix the format: turn each float point into its six uint16 fields, with the Doppler index from velocity and the cfg's velocity step, and the peak from SNR. No converter can fix the data. The models learned near-field hand motion at 10 frames a second on a different antenna array, and our walks are people at 1 to 5 m at 4 frames a second.

**What to try next.**
- Run the board at 100 ms frames (`frameCfg 2 0 200 64 100 0`, chirps unchanged), after checking for the hang TI's 100 ms tracking cfg caused on the shipped 05.04 image, and record the nine gestures at 20 to 50 cm, about 20 of each, with the repo's own logger reading a converted stream.
- Retrain its smallest model on those recordings, then score it on held-out gestures before claiming anything.
