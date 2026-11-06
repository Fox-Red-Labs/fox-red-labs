"""Describe an exported app folder in the open formats training tools and data catalogs read. Standard library only.
Called by export.py for each app; reads the files already written and adds, beside them:
  splits/train.csv, splits/test.csv   the training table cut by its split column (whole segments or devices)
  lake/app=<app>/date=<day>/records.jsonl   the records, Hive-style partitions by event date (Athena, Spark, DuckDB)
  croissant.json      MLCommons Croissant 1.0: files with sha256, every column typed, the label marked cr:Label
  datapackage.json    Frictionless Data Package with a Table Schema per CSV
  README.md           a Hugging Face dataset card whose YAML sets the configs and splits
Field names follow the schema; nothing here guesses a column it was not given."""
import csv
import hashlib
import json
import os

TYPES = {  # schema type or column kind -> (Croissant dataType, Frictionless type)
    "int": ("sc:Integer", "integer"), "float": ("sc:Float", "number"), "enum": ("sc:Text", "string"),
    "text": ("sc:Text", "string"), "time": ("sc:DateTime", "datetime")}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 16), b""):
            h.update(b)
    return h.hexdigest()


def kind_of(col, app):
    """The type of a column: from the schema when it is a field, from the name for bookkeeping and window features."""
    by_name = {f["name"]: f["type"] for f in app["fields"]}
    if col in by_name:
        return by_name[col]
    if col.endswith("_utc"):
        return "time"
    if col.rsplit("_", 1)[-1] in ("mean", "min", "max") and col.rsplit("_", 1)[0] in by_name:
        return "int"
    return "text"


ABOUT = {"device": "the device's Bluetooth address", "device_name": "the name the tag advertises",
         "event_time_utc": "when the device took the reading (UTC)", "start_utc": "start of the window (UTC)",
         "label": "the class, from a label file: what happened, not what the device said",
         "device_said": "the device's own answer, kept as the baseline to beat",
         "split": "train or test, by whole segment or device", "window": "window id: device, segment, first sample",
         "record": "record id: device and the device's own record number"}


def about(col, app):
    f = next((f for f in app["fields"] if f["name"] == col), None)
    if f:
        return f.get("about", "") + (" (%s)" % f["unit"] if f.get("unit") else "")
    base, stat = (col.rsplit("_", 1) + [""])[:2]
    if stat in ("mean", "min", "max"):
        return "%s of %s over the window" % (stat, base)
    return ABOUT.get(col, "")


def header(path):
    with open(path, newline="") as f:
        return next(csv.reader(f), [])


def write_splits(od):
    """gold.csv cut into splits/<split>.csv, rows unchanged, so tools that want one file per split get one."""
    gold = os.path.join(od, "gold.csv")
    if not os.path.exists(gold):
        return []
    rows = list(csv.DictReader(open(gold, newline="")))
    cols = header(gold)
    names = []
    for split in sorted({r["split"] for r in rows}):
        p = os.path.join(od, "splits", split + ".csv")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", newline="") as f:
            w = csv.DictWriter(f, cols)
            w.writeheader()
            w.writerows(r for r in rows if r["split"] == split)
        names.append(split)
    return names


def write_lake(od, app, records):
    """records: envelopes for this app. One JSONL per event day, folders lower-case key=value."""
    days = {}
    for r in records:
        days.setdefault((r.get("event_time") or "unknown")[:10], []).append(r)
    for day, recs in sorted(days.items()):
        p = os.path.join(od, "lake", "app=" + app["name"], "date=" + day, "records.jsonl")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.writelines(json.dumps(r) + "\n" for r in recs)
    return sorted(days)


def croissant(od, app, meta, splits):
    files, rsets = [], []

    def file_object(rel, fmt="text/csv"):
        files.append({"@type": "cr:FileObject", "@id": rel, "name": rel, "contentUrl": rel,
                      "encodingFormat": fmt, "sha256": sha256(os.path.join(od, rel))})

    def record_set(rid, src, cols, desc, key=None):
        fields = []
        for c in cols:
            dt = TYPES[kind_of(c, app)][0]
            fields.append({"@type": "cr:Field", "@id": "%s/%s" % (rid, c), "name": c, "description": about(c, app),
                           "dataType": [dt, "cr:Label"] if c == "label" else dt,
                           "source": {src[0]: {"@id": src[1]}, "extract": {"column": c}}})
        rs = {"@type": "cr:RecordSet", "@id": rid, "name": rid, "description": desc, "field": fields}
        if key:
            rs["key"] = {"@id": "%s/%s" % (rid, key)}
        rsets.append(rs)

    file_object("samples.csv")
    record_set("samples", ("fileObject", "samples.csv"), header(os.path.join(od, "samples.csv")),
               "One row per record as the hub logged it, every field by name, with its label if one is known.")
    if splits:
        for s in splits:
            file_object("splits/%s.csv" % s)
        files.append({"@type": "cr:FileSet", "@id": "splits", "name": "splits", "encodingFormat": "text/csv",
                      "includes": "splits/*.csv"})
        cols = header(os.path.join(od, "gold.csv"))
        record_set("training", ("fileSet", "splits"), cols,
                   "The training table: one row per window (time series) or per record, features, the label, the "
                   "device's own answer and the split. Split by whole segment or device, never by row.", key=cols[0])
    files.append({"@type": "cr:FileSet", "@id": "records", "name": "records", "encodingFormat": "application/jsonlines",
                  "includes": "lake/**/*.jsonl"})
    doc = {
        "@context": {"@language": "en", "@vocab": "https://schema.org/", "sc": "https://schema.org/",
                     "cr": "http://mlcommons.org/croissant/", "rai": "http://mlcommons.org/croissant/RAI/",
                     "dct": "http://purl.org/dc/terms/", "citeAs": "cr:citeAs", "samplingRate": "cr:samplingRate",
                     "equivalentProperty": "cr:equivalentProperty", "column": "cr:column",
                     "conformsTo": "dct:conformsTo", "data": {"@id": "cr:data", "@type": "@json"},
                     "dataType": {"@id": "cr:dataType", "@type": "@vocab"}, "examples": {"@id": "cr:examples", "@type": "@json"},
                     "extract": "cr:extract", "field": "cr:field", "fileProperty": "cr:fileProperty", "fileObject": "cr:fileObject",
                     "fileSet": "cr:fileSet", "format": "cr:format", "includes": "cr:includes", "isLiveDataset": "cr:isLiveDataset",
                     "jsonPath": "cr:jsonPath", "key": "cr:key", "md5": "cr:md5", "parentField": "cr:parentField",
                     "path": "cr:path", "recordSet": "cr:recordSet", "references": "cr:references", "regex": "cr:regex",
                     "repeated": "cr:repeated", "replace": "cr:replace", "separator": "cr:separator", "source": "cr:source",
                     "subField": "cr:subField", "transform": "cr:transform"},
        "@type": "sc:Dataset", "conformsTo": "http://mlcommons.org/croissant/1.0",
        "name": "%s-field-data" % app["name"], "description": meta["description"], "license": meta["license"],
        "url": meta["url"], "version": meta["version"], "datePublished": meta["date"],
        "creator": {"@type": "sc:Organization", "name": meta["creator"]},
        "citeAs": "@misc{%s_field_data, title={%s field data}, author={%s}, year={%s}, version={%s}, url={%s}}" % (
            app["name"].replace("-", "_"), app["title"], meta["creator"], meta["date"][:4], meta["version"], meta["url"]),
        "keywords": ["field data", "time series" if app["training"]["task"].startswith("timeseries") else "tabular",
                     "edge AI", "Bluetooth LE", app["name"]],
        "distribution": files, "recordSet": rsets}
    json.dump(doc, open(os.path.join(od, "croissant.json"), "w"), indent=1)


def datapackage(od, app, meta, splits):
    res = []
    for rel in ["samples.csv"] + ["splits/%s.csv" % s for s in splits]:
        path = os.path.join(od, rel)
        res.append({"name": rel.replace("/", "-").replace(".csv", ""), "path": rel, "format": "csv", "mediatype": "text/csv",
                    "hash": "sha256:" + sha256(path), "bytes": os.path.getsize(path),
                    "schema": {"fields": [{"name": c, "type": TYPES[kind_of(c, app)][1], "description": about(c, app)}
                                          for c in header(path)]}})
    json.dump({"name": "%s-field-data" % app["name"], "title": app["title"] + " field data", "description": meta["description"],
               "version": meta["version"], "created": meta["date"] + "T00:00:00Z", "licenses": [{"name": meta["license"]}],
               "contributors": [{"title": meta["creator"], "role": "author"}], "resources": res},
              open(os.path.join(od, "datapackage.json"), "w"), indent=1)


def hf_card(od, app, meta, splits, counts):
    ts = app["training"]["task"].startswith("timeseries")
    lines = ["---", "pretty_name: %s field data" % app["title"], "license: %s" % meta["license"].lower(),
             "tags: [%s, edge-ai, field-data]" % ("timeseries" if ts else "tabular"), "configs:"]
    if splits:
        lines += ["- config_name: default", "  data_files:"] + \
                 ["  - split: %s\n    path: \"splits/%s.csv\"" % (s, s) for s in splits]
    lines += ["- config_name: samples", "  data_files: \"samples.csv\"",
              "---", "",
              "# %s field data" % app["title"], "", meta["description"], "",
              "| Config | What it holds | Rows |", "|---|---|---|"]
    if splits:
        lines.append("| `default` | the training table, %s | %s |" % (", ".join(splits), ", ".join(
            "%d %s" % (counts.get(s, 0), s) for s in splits)))
    lines += ["| `samples` | one row per record, every field by name | %d |" % counts.get("samples", 0),
              "", "`lake/app=%s/date=<day>/records.jsonl` holds the same records as full envelopes for a warehouse or data "
              "lake (Athena, Spark, DuckDB read the `key=value` folders as columns). It is not a loader config here: the "
              "Hub loader picks one file format per dataset, and the configs above are CSV." % app["name"], "",
              "Label classes: %s. `label` is from a label file; `device_said` is the device's own answer, the baseline a "
              "model has to beat." % ", ".join(app["label"]["classes"]), "",
              "Machine-readable descriptions beside this card: `croissant.json` (MLCommons Croissant 1.0) and "
              "`datapackage.json` (Frictionless). Field meanings and units: `../schema.json`. How it was collected and what "
              "it is not for: `../DATASHEET.md`.", ""]
    open(os.path.join(od, "README.md"), "w").write("\n".join(lines))


def describe(od, app, records, meta):
    """Write splits, lake, croissant.json, datapackage.json and README.md for one app folder."""
    splits = write_splits(od)
    days = write_lake(od, app, records)
    counts = {"samples": len(records)}
    for s in splits:
        counts[s] = sum(1 for _ in open(os.path.join(od, "splits", s + ".csv"))) - 1
    croissant(od, app, meta, splits)
    datapackage(od, app, meta, splits)
    hf_card(od, app, meta, splits, counts)
    return days
