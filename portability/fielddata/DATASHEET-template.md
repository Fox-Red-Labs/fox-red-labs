# Datasheet: field dataset

In the shape of "Datasheets for Datasets" (Gebru et al., 2018). Written by `portability/fielddata/export.py` on {{exported}}.

## What is in it

{{summary}}

`records.jsonl` holds every record in one envelope: app, schema version, device, hub, kind, the record's own id, when it happened (`event_time`), when the home base received it (`ingest_time`), how it arrived (`source`: phone, modem or wifi), the fields by name, and the label if one is known. `schema.json` gives every field's type, unit and meaning. Each app has its own folder with a flat table (`samples.csv`), a training table (`gold.csv`) and the same data laid out for TI's Tiny ML Tensorlab and Edge AI Studio (`tensorlab/`), for Edge Impulse (`edge-impulse/`) and, for time-series apps, the capture format `tools/dataset/` reads (`capture/`).

**No person is in it.** Devices are named by their Bluetooth address. Tying a device to a worker is the deploying organisation's decision, and in some places it needs the workers' agreement.

## How it was collected

Device MCU over UART to a Bluetooth LE tag, the tag's advertising to a hub, the hub's log to the home base by a phone, a modem line or the hub's own Wi-Fi. Records can be lost before the hub hears them; advertising carries only the latest sample, so time-series apps lose samples when they send faster than the hub listens (the training windows skip gaps).

## Labels

From label files: intervals (`device,start_utc,end_utc,label`) written by a person, a door switch, a test rig or a simulator, or per record (`device,record_id,label`). `device_said` in `gold.csv` is what the device's own logic or model reported; it is the baseline to beat, never the truth.

## Splits

By segment or by device, never by row, so neighbouring samples never straddle the split.

## What it is for, and not for

For training and testing models for the app, and for studying how devices behave in the field. Not for judging people. Not a safety system.
