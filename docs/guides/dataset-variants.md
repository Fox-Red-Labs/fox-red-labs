# How do I collect washer vibration (BMI160 on BP-BASSENSORSMKII) or sound clips (LP-MSPM0G5187) for a TI MSPM0 model, and which datasets does TI already ship?

Washer phases use the same tools as gestures: tape the board to the lid and press a key when the machine changes phase. Sound clips on the LP-MSPM0G5187 are labeled by folder instead of by key. And most of TI's AI examples already carry stored test data that shows the window shape each model expects. The main guide, with the tools and every step: [docs/guides/collect-a-dataset.md](collect-a-dataset.md).

**Status (2026-10-08).** None of this has been recorded on a board: the BP-BASSENSORSMKII sensor BoosterPack plug-in module and the LP-MSPM0G5187 have not arrived. The tables of TI's datasets are read from the SDKs installed on this Mac.

Terms: [docs/vocab.md](../vocab.md) and the terms in [the main guide](collect-a-dataset.md). Cause tags: **Unknown** (not found or not run; "suspected" names the lead), **Setup or handling** (how the board is set up or mounted). **Harmonic**: a vibration at a whole multiple of the main one. **Regression**: a model that outputs a number instead of a class. 1 kB = 1,024 bytes.

## Can I record "is the washer running" the same way?

Yes, with the same tools. Tape the LaunchPad and BoosterPack to the washer lid, power it from a USB battery or a long cable, and use keys such as `o=off,w=wash,p=spin`. A key press sets the label until the next one, so you only press when the machine changes phase. One wash cycle is one recording; record three different cycles. At 100 samples a second a recording can show shaking up to 50 Hz. A drum at 1,200 rpm shakes at 20 Hz, and its second harmonic (40 Hz) is close to that limit, so try 200 Hz, which shows up to 100 Hz. No washer has been recorded, so 200 Hz is a starting guess, not a tested setting. To change it, in `bmi160_init` (`core/bmi160.c`) change the write to register 0x40 (the sensor's `ACC_CONF` register) from 0x28 to 0x29, change the write to register 0x42 (`GYR_CONF`) from 0x28 to 0x29 so the gyroscope keeps up, and set the rate in `ds_init` to 200. Six channels at 200 Hz still fit the serial link: at most 52 bytes a line is 10.2 kB a second, against 11.3 kB at 115200 baud. TI's G3519 `washer_load_ai` example estimates the load from motor data instead; it needs no sensor but cannot tell you the phase of your own machine.

## How do I collect sound clips on the LP-MSPM0G5187?

For sounds, label by file instead of by key: one folder per class, one short clip per file (for example `clips/knock/knock_001.wav`). Two ways to get audio from the G5187:

- **The onboard microphone through TI's USB microphone example.** `examples/nortos/LP_MSPM0G5187/tinyusb/device_uac_microphone_i2s` makes the board a USB microphone: its `tusb_config.h` sets 48k samples a second, one channel, 2 bytes per sample, and its product string is `MicNode`. Plug a second cable into the chip's own USB-C port (bottom right), pick it as the input in Audacity or macOS's QuickTime Player, and record. Not run: the board has not arrived.
- **TI's wake-word input.** `edgeAI/wakeword_detection_ai_npu` samples an analog microphone on header pin 6 at 8 kHz and turns each 20 ms frame into 70 features. Record at 8 kHz from the same microphone if you want your clips to match what that model hears.

Keep every clip the same length and rate (1 s at 16 kHz mono 16-bit matches Google Speech Commands, below). This prints rate, channels and length for every clip, using only Python's standard library:

```bash
python3 -c "import wave,glob;[print(f, w.getframerate(), w.getnchannels(), round(w.getnframes()/w.getframerate(),2)) for f in sorted(glob.glob('clips/*/*.wav')) for w in [wave.open(f)]]"
```

Balance and recording rules are the same: similar clip counts per class, and test clips from a recording recording the model never trained on.

## Datasets TI already ships

TI's AI examples run on stored test data, so most need no sensor. That data is a good template for your own: it shows the window shape and the channel layout each model expects. From MSPM0 SDK 2.11.00.07 (`examples/nortos/LP_<board>/edgeAI/`):

| Example | Boards | Stored input | Classes |
|---|---|---|---|
| `hello_world_ai` | G3507, G3519, G5187 | 128 samples, one channel (`hello_world_testvector.h`) | sawtooth, sine, square |
| `gearbox_fault_diagnosis_ai` | G3507, G3519, G5187 | 4 vibration channels by 256 samples, from the public Kaggle "Gearbox Fault Diagnosis" data (SpectraQuest simulator) | healthy, broken tooth |
| `electrical_fault_ai_npu` | G5187 | 6 channels (currents and voltages) by 128 | no fault and five fault types |
| `linear_actuator_fault_ai` | G3519, G5187 | 3 channels by 1,600 | normal, spalling, lack of lubrication, backlash |
| `glass_break_detection_ai_npu` | G5187 | one clip of 24,000 16-bit samples | glass break, normal, similar sound |
| `machine_readable_code_classification_ai_npu` | G5187 | 28 by 28 image | QR code, barcode, other |
| `motor_temperature_ai`, `washer_load_ai` | G3519 (G5187 for the first) | motor signals | numbers, not classes (regression) |

C2000Ware 26.02 (`libraries/ai/examples/`), the MSPM33 SDK 1.04 and the AM13E230x SDK 26.01 ship `test_vector.c` files with golden outputs made by TI's ModelMaker, for arc fault, motor bearing fault (six classes), blower imbalance, fan blade anomaly and others. The `*_data_capture` examples (such as `timeseries_data_capture`, which samples the ADC at 5 kHz and sends it at 115200 baud) and `*_live_preview` examples stream to Edge AI Studio instead of to a CSV file; they are TI's own route to the same goal.

## Public datasets and their licenses

| Dataset | What it is | License (read 2026-10-08) |
|---|---|---|
| UCI Room Occupancy Estimation, id 864, <https://archive.ics.uci.edu/dataset/864/room+occupancy+estimation> | 10.1k rows, one every 30 s: light, sound, CO2, PIR and temperature; 0 to 3 people. The four occupancy models were trained on it | CC BY 4.0: credit Singh and Chaudhari and the UCI repository |
| Google Speech Commands v0.02, <https://arxiv.org/abs/1804.03209> | 105,829 one-second clips of 35 words; download 2.26 GB from `storage.googleapis.com/download.tensorflow.org/data/speech_commands_v0.02.tar.gz` | CC BY 4.0, stated in the paper |

## Three questions an accuracy number can answer, and which one you are reading

A model page here can carry three different numbers, and they must not be read as one (adopted 2026-10-09):

1. **Did the port reproduce the model?** The 20 stored test vectors (occupancy) or the 10 shipped digits (BitNetMCU) give the same class on the board as on the Mac. That is a functional check of the port and the compiler. It says nothing about how often the model is right. Class agreement is also not identical numbers: an int8 NPU build can differ from the CPU build by a count in a logit and still agree on the class, and the pages say so where it happened.
2. **How accurate is the model on data it did not train on?** A held-out metric with the split named. The occupancy models' published 90.9 percent (8k model, int8) is on 1,001 windows cut in 48-minute blocks from the same room and the same days as the training data; neighboring blocks share conditions, so it is an optimistic figure. The stricter protocol holds out whole days: `tools/dataset/eval_occupancy.py --split day` freezes the split (test days 2018/01/10 and 2018/01/11) before any training, writes the windows for `train.py`, scores a model trained on them, and refuses to score when train and test share a block or day, when the windows do not match the model's input, or when a label is missing. The record it writes carries the dataset and model hashes, the window counts, the class distribution and the metric definitions. The dataset is one room: no split here measures a new room.
3. **Does it work in a real room?** Only a recorded trial with live sensors answers this, and none has been run for the occupancy models. The digit demo read finger-drawn digits on the touch screen, but nobody counted hits and misses, so it has no real-world accuracy either.

## Questions

### Is 200 Hz the right rate for a washer?

Start there and check the plot. It is a starting guess from the drum speed, not a tested setting: no washer has been recorded. If the plot of the spin phase looks smooth with no fast wave, the rate is high enough; if it looks jagged at every sample, try higher.

### My washer phases all look the same in the plot. What now?

Mount the board more firmly and record again. Not seen here; a loose board or soft tape damps the shaking. Tape it flat to a hard panel, not the lid's soft edge, and keep the same spot for every recording.

### The LP-MSPM0G5187 gives no audio as a USB microphone. Why?

 Check that the second USB cable is in the chip's own USB-C port (bottom right), not the probe's, and that `MicNode` is picked as the input.
