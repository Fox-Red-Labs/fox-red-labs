# Collect field data from any device, keep it the same way, and train on it

An open, small framework for getting sensor data from devices in the field into a dataset you can train on. A device sends numbered fields over a UART. A Bluetooth tag advertises them. A hub collects every device nearby. A phone or an uplink carries the records home. Every record ends up in one envelope, labelled, and exported in the layouts that TI's Tiny ML Tensorlab and Edge AI Studio, Edge Impulse and this repository's own `tools/dataset/` read. A new kind of device is one JSON schema file; the tag, the hub and the phone app do not change.

Terms: [docs/vocab.md](../../docs/vocab.md). The reference system that runs it: [tool-telemetry/](../../tool-telemetry/README.md) (CC2340R5 tag, CC35X1 hub, phone app, home base).

| App | Schema | Device | Status |
|---|---|---|---|
| Power tools | `apps/tool-telemetry.json` | simulated drill | runs end to end; uses its own fixed frame on the air |
| Room presence | `apps/presence.json` | simulated radar through the CC2340R5 tag | runs end to end over the generic frame |

## The pieces

| File | What it is |
|---|---|
| `apps/<name>.json` | One app: its id (1 to 255), its fields (id, name, type, unit, enum words), what to show first, which field is the label and its classes, and how to train (`timeseries_classification` with channels, rate, window and stride, or `tabular_classification`) |
| `c/fd_frame.c`, `.h` | The generic frame for devices, tags and hubs: parse a `$FD` line, build the advertising data, decode it. No heap, about 120 lines |
| `fd.py` | The same in Python, plus the schemas, the record envelope and decoding by name |
| `export.py` | Home base data to `records.jsonl`, per-app `samples.csv`, `gold.csv` and `splits/`, a dated `lake/`, and the Tensorlab, Edge Impulse and capture layouts, with a datasheet |
| `cards.py` | Describes each app folder in open formats: Croissant 1.0 (`croissant.json`), Frictionless (`datapackage.json`) and a Hugging Face card (`README.md`) |
| `tensorlab.py` | A TI Tiny ML Tensorlab config and the dataset zip Edge AI Studio imports, from the app's schema, with the knobs to tune |
| `CHAIN.md` | The whole chain in six steps: collect, save, describe, upload, train, deploy |
| `train.py` | A small decision tree on any app's `gold.csv`, written as C, scored against the device's own answer |
| `tests/test_fd.py` | C against Python on 8,000 lines and frames |

## On the wire

From the device to the tag, one line per sample, 115200 baud:

```text
$FD,n=42,app=2,1=1,2=87,3=240,4=930,5=310,6=1,7=31*5C
```

`n` counts lines, `app` is the app id, and every other key is a field id with an integer value. The two hex digits are the XOR of the bytes between `$` and `*`. A field the schema does not know is kept and passed on; a line with a bad checksum is dropped and counted.

On the air, the tag puts the fields in its manufacturer data: `FF FF 'F' 'R' 0x21 <app> <seq>`, then for each field its id, its length (1, 2 or 4 bytes) and the value, signed, little-endian. A legacy advertisement fits the first few fields; the extended one fits all 16.

## The record

Every record from every app, in `records.jsonl` and at `POST /api/ingest`, has the same shape:

```json
{"app": "presence", "app_id": 2, "schema": 1, "device": "40:F3:B0:xx:xx:xx", "hub": "FRL-HUB-3250",
 "kind": "sample", "id": "17:412", "event_time": "2026-10-17T16:40:02Z", "ingest_time": "2026-10-17T16:42:36Z",
 "source": "wifi", "fields": {"state": "moving", "confidence_pct": 87, "range_cm": 240, "motion_energy": 930,
 "still_energy": 310, "people": 1, "temp_c": 31}, "label": "moving"}
```

Two clocks on purpose: when it happened, and when it arrived. `source` says which path brought it (phone, modem or wifi), and the home base stores a record once however many paths deliver it.

## Labels

A label file is what a person writes down while watching, or what a door switch, a test rig or a simulator writes:

```text
device,start_utc,end_utc,label
40:F3:B0:xx:xx:xx,2026-10-17T16:40:00Z,2026-10-17T16:41:30Z,moving
```

or one label per record (`device,record_id,label`). The device's own answer (`state` here) is kept beside the label as `device_said`: it is the baseline a trained model has to beat, never the truth.

## Add an app

1. Write `apps/<name>.json` with a new id. Copy `presence.json`.
2. Have the device send `$FD,app=<id>,...` lines to a tag's UART.
3. Rebuild the hub only to refresh the phone app's copy of the schemas (`tool-telemetry/gateway/build.sh`); the home base reads them at start.
4. Record, label, `export.py`, `train.py`.

## Export and train

The full path, with the formats and the upload and deploy steps: [CHAIN.md](CHAIN.md).

```bash
python3 portability/fielddata/export.py --labels labels.csv --out field-dataset
python3 portability/fielddata/train.py field-dataset/presence/gold.csv
python3 portability/fielddata/tensorlab.py field-dataset/presence --device MSPM0G5187
```

`export.py` reads the home base's SQLite database; pass its path with `--db`. For TI's tools, zip `<app>/tensorlab/` (it holds `classes/<label>/*.csv` and `annotations/`) and point Tensorlab's `dataset.input_data_path` at it, or upload it to Edge AI Studio's Model Composer; TI's tools compile the model for its MCUs. Tensorlab trains on time series only, so the tool app's per-pull rows there are a best effort; the decision tree here, or Edge Impulse's tabular upload, fits them better.

## Results

Bench export of 2026-10-17: simulated devices through a real CC2340R5 tag and CC35X1 hub.

| App | Records | Training rows (train / test) | Tree here | Device's own answer | Open formats checked |
|---|---|---|---|---|---|
| Room presence | 870, 831 labelled | 279 / 61 windows | 100% | 100% | Croissant validates and loads, Frictionless valid, Hugging Face loads, DuckDB reads the lake |
| Power tools | 269 pulls, all labelled | 206 / 63 pulls | 95.2% | 96.8% | same |

Simulated data, so the numbers show the chain works, not that the models are good. The presence test split holds no quiet segments.

## Limits

Advertising carries the latest sample only, so the hub logs about one sample a second whatever the device sends; faster time series need the tag to batch samples per frame (not built). The Tensorlab layout and `config.yaml` follow TI's documentation and have not been run in Tensorlab here. Nothing is encrypted on the air.
