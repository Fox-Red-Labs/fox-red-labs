# Datasheet: tool telemetry dataset from LP-EM-CC2340R5 tags and an LP-EM-CC35X1 hub

Terms: [docs/vocab.md](../../docs/vocab.md). Written in the shape of "Datasheets for Datasets" (Gebru et al., 2018). Copied into every export by `ai/dataset.py`.

## Why it exists

To learn how motor-driven equipment is used at a work site, one run at a time: how hard each run works the motor and battery, how often a run jams, how heat builds over a shift. The first model built on it says, on the tool, whether a run was light, normal, heavy or a jam.

## What is in it

| Table | One row per | Use |
|---|---|---|
| `bronze/*.jsonl` | upload to the home base, as received | audit and replay; never edited |
| `silver/snapshots.csv` | change in a tool's counters (trigger pulls, trips, faults, run time, battery) | usage over time, fleet reports |
| `silver/events.csv` | event the tag logged (pull ended, overcurrent trip, fault, restarts) | alerts, service history |
| `silver/pulls.csv` | trigger pull, with its summary from the tool MCU | the training unit |
| `gold/pulls.csv` | trigger pull, with derived features, context, label and split | training and evaluation |

Every column, its type and unit: `schema.json`. Times are UTC, ISO 8601. Each row keeps where it came from (`hub_id`, `source`) and both clocks: when it happened (`event_time_utc`) and when the home base got it (`ingest_time_utc`). A record that arrived by two paths (a phone and a modem) is stored once.

**No person is in it.** There is no user, badge or phone identifier. A tool is named by its tag's Bluetooth address and the tool MCU's name. Tying tool use to a worker is a decision for the equipment owner, and in some places it needs the workers' agreement (Germany's works councils decide on any system able to monitor behavior; New York, Connecticut and Delaware require notice of electronic monitoring).

## How it was collected

Tool MCU over UART to a Bluetooth LE tag, the tag's advertising to a site hub, the hub's log to the home base by a phone, the hub's UART modem line, or the hub's site Wi-Fi. Formats: `PROTOCOL.md`. Records can be lost before the hub hears them (the hub counts gaps in the tag's pull numbers as `pmiss`); nothing is lost after the hub logs it unless the hub loses power before a phone or uplink collects it.

**Simulated or real.** Until the AM13E230 motor rig reports, the tool is `sim/toolsim.py`, drawing each pull from `ai/physics.py`. Simulated rows have `label_source = simulator`. Real rows will have no label until a person tags them or a test rig sets the load.

## Labels

`label` is light, normal, heavy or jam. From the simulator it is exact. On real tools the sources are: a test rig that sets the load (a brake on the shaft), a person tagging pulls on video, or the tool's own overcurrent trip for jams. `model_class` is what the tag's model said; never train on it as if it were the truth.

## Splits

By tool, never by row: a hash of `tool_id` puts about one tool in five in `test`. A model scored on pulls from tools it trained on looks better than it is.

## What it is for, and not for

For: tool-use reports, load classification, jam detection, battery and thermal studies, comparing tools across crews.
Not for: judging a worker's performance, pay or discipline. Not a safety system: the tool's own overcurrent trip stays the safety path, whatever a model says.

## Keeping it

Bronze is append-only. Silver and gold are rebuilt from bronze by `ai/dataset.py`, so a fix to the cleaning code is applied by running it again. Add a column at the end and bump nothing; rename or change a meaning and note it here with the date.
