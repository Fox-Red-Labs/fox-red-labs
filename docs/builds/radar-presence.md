# See whether a room is occupied with an IWRL6432BOOST 60 GHz radar

TI's IWRL6432BOOST streams presence and range frames over USB with no setup. A short script reads them and a timed test scores detection, delay and false alarms.

The page: [site/project-radar-presence.html](../../site/project-radar-presence.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's Motion and Presence demo, read and timed

The IWRL6432BOOST as shipped, a USB cable, a Mac, and a person to walk and sit.

Status: Works. Boards: iwrl6432boost.

Folders this version uses (in a sparse clone, fetch them first, then read `iwrl6432boost/AGENTS.md`):

```bash
git sparse-checkout add iwrl6432boost tools
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| IWRL6432BOOST 60 GHz mmWave radar evaluation module | 1 | the radar board |
| Micro-USB B cable, a stand and a tape measure | 1 | power and data; the stand and tape for the timed test |

### Before you start

- A Mac with Python 3 and pyserial. No TI tools are needed to read the shipped stream. The timed test speaks its cues with macOS `say`.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version.
- Keep 20 cm between the board and a person while it transmits.

### Steps

1. **Bench.** Plug the board's micro-USB port (J5) into the computer. Leave the switches as shipped. Keep 20 cm between the board and a person while it transmits (TI's board guide).

   You should see: Lights D6 (5 V) and D3 (power good) come on; the antenna side faces the room.

   No lights: try another cable; the port is micro-USB B.

2. **Terminal (Mac).** The board adds two serial ports; the one ending in 1 carries the data.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two ports, one ending in `1` and one in `4`.

   No ports listed: check the cable is a data cable and the board is powered.

3. **Terminal (Mac).** Make a venv with pyserial, then capture 15 s at 115,200 baud from the port ending in 1 into a file with any serial tool.

   ```text
   python3 -m venv ~/radar-venv && ~/radar-venv/bin/pip install pyserial
   ```

   You should see: A capture file of about 6 kB (4 frames a second, 384 bytes each).

   An empty file: check that you read the port ending in 1, then press the board's reset button (S2) and capture again.

4. **Terminal (Mac).** The parser prints one line per frame. Test it without a board first with `--selftest`.

   ```text
   python3 iwrl6432boost/tools/tlv_parse.py capture.bin
   ```

   You should see: Frame number, bytes, detected objects and the block types; presence blocks are type 315 and range profiles 303.

   The self test fails: you are not at the top of the repository.

5. **Terminal (Mac).** Open `iwrl6432boost/eval/radar_link.py` and set `PORT` and `CFG` (written for this bench's folders) to your port and configuration folder. Then apply each setting and check the stream.

   ```text
   python3 iwrl6432boost/eval/radar_eval.py accept
   ```

   You should see: The board's answer to every configuration line, then a stream check for 1x1 (refused with error -40107), 2x2 and 2x3.

   The demo ignores its command line until one press of the board's reset button (S2) after power-up.

6. **Terminal (Mac) and the room.** Put the board on a stand at about 1.5 m, antenna toward the room, a tape line out from it, and two chairs at 2 m and 4 m. Leave the room for the empty run, then run the cued walk-in and seated passes; Enter in the terminal confirms that the person is in place, and the tool speaks the cues.

   ```text
   python3 iwrl6432boost/eval/radar_eval.py empty --seconds 60
   python3 iwrl6432boost/eval/radar_eval.py run --passes walk,sit2,sit4
   ```

   You should see: A results file per setting in `iwrl6432boost/eval/results/`: flags in the empty room, detect delay after the cue, hold while seated, and the distance at first flag on the walk-in.

   Put the shipped configuration back afterwards with `restore`.

7. **Terminal (Mac).** Restore the shipped configuration.

   ```text
   python3 iwrl6432boost/eval/radar_eval.py restore
   ```

   You should see: The board prints the shipped lines again and streams at about 4 frames a second.

   Press reset (S2) if it does not answer.

### Expected result

The board streams presence and range frames about four times a second. In the timed test a seated person at 2 m and 4 m is held as present on 2x2 and 2x3 (100 percent of frames, one pass each).

### What this shows

- A person sitting still at 2 m and 4 m was held in 100 percent of frames on 2x2 and 2x3. An arrival was flagged 4.7 to 6.2 s after the cue, which includes the person's reaction and walk. With a modified configuration (PresenceDetect-hold12.cfg) the empty room gave 0 flags in 6.08 h, until a flag whose cause was not checked.
- A short Python parser reads the frames, and the timed test needs no change to the board's firmware.

### Limits

- How many people are in the room: the demo reports four floor zones on or off.
- Anything past 4 m seated or about 7 m walking: not tested.
- Your room: glass, metal shelving and fans change the result. One room, one afternoon, one pass per cell, so no spread.
- Power use: the demo does not report it and no measurement was made.
- The steps are from the 2026-10-09 and 2026-10-10 runs.
- The tool's serial port and configuration paths are written for this bench (`PORT`, `CFG` in `radar_link.py`).
- Checked on one Intel Mac only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Frame rate | 4.00 a second as shipped; 3.85 a second with the evaluation setting | `iwrl6432boost/README.md` |
| Range bin | 0.121 m (derived from the settings, not checked against a measured distance) | `docs/guides/evaluate-a-presence-radar.md` |
| Chip | IWRL6432, 2 transmit and 3 receive channels, 57 to 64 GHz; Cortex-M4F core | `iwrl6432boost/README.md` |
