# From the field to a trained model and back: the field-data chain

Terms: [docs/vocab.md](../../docs/vocab.md). Part of [field data](README.md).

Six steps, each with one command and one open format, so the data lands in whatever store and training tool a team already uses. Every step below ran on 2026-10-17 on data from simulated devices sent through a real CC2340R5 tag and CC35X1 hub. The one exception is the Tensorlab run itself (step 5), which has not been run here.

| Step | Where it lives | Format | Who reads it |
|---|---|---|---|
| 1. Collect | the hub's log, then the home base database | the record envelope (README, "The record") | the phone app, the home base |
| 2. Save | `lake/app=<app>/date=<day>/records.jsonl` | JSON Lines, Hive-style folders | Athena, Spark, DuckDB, BigQuery external tables |
| 3. Describe | `croissant.json`, `datapackage.json`, `README.md` | MLCommons Croissant 1.0, Frictionless, Hugging Face card | Hugging Face, Kaggle, OpenML, TensorFlow Datasets, PyTorch, data catalogs |
| 4. Upload | an S3 bucket, a Hugging Face dataset repo, or any file share | the same folder, unchanged | your cloud |
| 5. Train | `tensorlab/config.yaml` and `<app>_field.zip` | TI Tiny ML Tensorlab and Edge AI Studio layout; Edge Impulse CSV; a small tree here | TI's tools, Edge Impulse, scikit-learn |
| 6. Deploy | the device | C from the trainer | the device MCU or the tag |

## 1. Collect

Devices send `$FD` lines (or the tool's fixed frame). The hub logs every record it hears. A phone on the hub's Wi-Fi, a modem line or the hub's own uplink carries them to the home base, which stores each record once:

```bash
python3 tool-telemetry/homebase/server.py            # the home base, on a laptop or a server
```

## 2. Save

```bash
python3 portability/fielddata/export.py --labels labels.csv --out field-dataset \
    --provenance "Simulated devices through a real tag and hub, 2026-10-17"
```

Per app this writes the raw records by day (`lake/`), one flat row per record (`samples.csv`), the training table (`gold.csv`, also cut into `splits/train.csv` and `splits/test.csv`), and the layouts for TI's tools and Edge Impulse. Labels come from a file a person, a test rig or a simulator writes; the device's own answer is kept beside the label as the baseline.

Keep the raw records forever and rebuild everything else from them. A warehouse reads the lake folder as a table with `app` and `date` as columns:

```sql
-- DuckDB, as run on the bench export
select app, date, count(*) n, count(label) labelled
from read_json_auto('*/lake/*/*/records.jsonl', hive_partitioning=true) group by all;
```

## 3. Describe

`export.py` writes three descriptions beside each app's files, so another tool can load the data without being told what a column means:

- `croissant.json`: MLCommons Croissant 1.0. Every file with its sha256, every column typed, the label marked `cr:Label`. Hugging Face, Kaggle, OpenML and TensorFlow Datasets read it.
- `datapackage.json`: Frictionless Data Package, a Table Schema per CSV.
- `README.md`: a Hugging Face dataset card. Its YAML sets the configs: `default` (train and test) and `samples`.

Checked on the bench export with each project's own tool:

```text
mlcroissant validate --jsonld croissant.json        # no errors, no warnings
mlcroissant load --jsonld croissant.json --record_set training --num_records 2
frictionless validate datapackage.json              # every resource VALID
python -c "import datasets; print(datasets.load_dataset('field-dataset/presence'))"
```

`schema.json` gives every field's type, unit and meaning, and `DATASHEET.md` says how the data was collected and what it is not for.

## 4. Upload

The export folder is the upload. Pick the store you already have; nothing about the files changes.

```text
# S3 (or any S3-compatible store)
aws s3 sync field-dataset/presence s3://<bucket>/field-data/presence/

# Hugging Face: create a private dataset repo first, then
hf upload <org>/presence-field-data field-dataset/presence . --repo-type=dataset
```

New days add new `date=` folders, so a nightly sync only moves what is new. The phone app and the home base can also post records straight to `POST /api/ingest` on your own server. Nothing in this repository uploads anywhere by itself.

## 5. Train, and change the model

Three ways, from smallest to TI's full flow.

**Here, in seconds.** A small decision tree, written out as C and scored against the device's own answer on the same test rows:

```bash
python3 portability/fielddata/train.py field-dataset/presence/gold.csv --depth 5
```

**TI Tiny ML Tensorlab, on the command line.** Generate the config and the dataset zip from the app's schema, then run TI's script:

```bash
python3 portability/fielddata/tensorlab.py field-dataset/presence --device MSPM0G5187 --model CLS_1k_NPU
./run_tinyml_modelzoo.sh field-dataset/presence/tensorlab/config.yaml
```

The knobs are lines in `config.yaml`. Change one at a time and compare runs:

| Knob | Key | Try |
|---|---|---|
| Target chip | `target_device` | the MCU that will run it, such as MSPM0G5187, AM13E2 or F28P55 (these three have an NPU) |
| Model size | `model_name` | one step: CLS_500_NPU, CLS_1k_NPU, CLS_2k_NPU |
| Training length | `training_epochs` | 20, then 50 if the loss is still falling |
| Step size | `learning_rate` | 0.001, then 0.0003 if the loss jumps |
| Window | `frame_size` | the schema's window; longer sees slower changes |
| Number format | `quantization`, `quantization_method` | 2 and PTQ, then QAT if accuracy drops after quantizing |

**TI Edge AI Studio Model Composer, in a browser.** Sign in at dev.ti.com/edgeaistudio, create a time series classification project for the same device, and use Import Annotated Archive Dataset on `<app>_field.zip`. Set the model and training values from `config.yaml`. Model Composer can export its settings as a YAML in the same shape, so the browser and the command line stay in step.

**Edge Impulse** takes `edge-impulse/` as a CSV upload (label in the file name). Any other trainer takes `splits/train.csv` and `splits/test.csv`.

## 6. Deploy, and keep score

From one Tensorlab run, copy `mod.a`, `tvmgen_default.h`, `test_vector.c` and `user_input_config.h` into the SDK's AI example in CCStudio, build and flash, and check the golden vectors pass before trusting a live answer. From `train.py`, add `fd_model.c` to the device or the tag; the tool tag here runs one that way.

The device then reports its own answer as a field, the hub collects it with everything else, and the next export puts it beside the next labels as `device_said`. That column is how a team knows whether the model on the device still holds up in the field, and when to train again.

## Bench results

| App | Records | Training rows (train / test) | Tree here, test accuracy | Device's own answer | Tensorlab |
|---|---|---|---|---|---|
| Room presence | 870 from 1 sensor, 831 labelled | 279 / 61 windows of 6 | 100% (9 nodes) | 100% | config and zip generated, not run |
| Power tools | 269 pulls from 1 tag, all labelled | 206 / 63 pulls | 95.2% (21 nodes) | 96.8% | config and zip generated, not run |

The data is simulated, so these numbers show the chain works, not that the models are good. The presence test split happens to hold no quiet segments, which is why both columns read 100%.

## Limits

- Tensorlab and Model Composer have not been run on these files. The keys follow TI's documentation read 2026-10-17. TI's pages disagree on the split key names, so the config leaves the split to `annotations/`.
- Tensorlab trains on time series. The tool app's per-pull rows are separate events, so its Tensorlab layout is a best effort; the tree here or Edge Impulse fit it better.
- The Hugging Face card covers the CSV configs. The JSON Lines lake is for warehouses: the Hub loader picks one file format per dataset.
- The upload commands are the vendors' standard ones and were not run here: nothing has left this machine.
