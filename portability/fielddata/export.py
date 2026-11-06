#!/usr/bin/env python3
"""Export what the home base collected, for every app, in the open record format and in the layouts training tools take.
Standard library only.
    python3 portability/fielddata/export.py [--db homebase.sqlite] [--labels labels.csv ...] [--out field-dataset/]

Writes, under --out:
  records.jsonl            every record from every app in one envelope (README.md, "The record")
  schema.json              the app schemas the records were decoded with
  <app>/samples.csv        one row per record: device, times, source, every field by name, label
  <app>/gold.csv           the training table: per window of samples (time-series apps) or per record (tabular apps),
                           with features, the label, the device's own answer for comparison, and a train/test split
  <app>/capture/<dev>.csv  time-series apps: the tools/dataset capture format (t_ms, channels, label), so
                           tools/dataset/check.py and train.py work on field data
  <app>/tensorlab/         TI Tiny ML Tensorlab and Edge AI Studio layout: classes/<label>/*.csv and annotations/
  <app>/edge-impulse/      Edge Impulse CSV upload layout: <label>.<sample>.csv
  <app>/splits/, lake/      gold.csv cut by split; the records as lake/app=<app>/date=<day>/records.jsonl
  <app>/croissant.json, datapackage.json, README.md   Croissant 1.0, Frictionless and a Hugging Face card (cards.py)
  DATASHEET.md             what the data is, how it was collected, and what it is not for
Labels: CSV files with device,start_utc,end_utc,label (intervals, what a person writes down) or device,record_id,label."""
import argparse
import csv
import hashlib
import json
import os
import re
import sqlite3
import statistics
import time
from datetime import datetime, timezone

import cards
import fd

HERE = os.path.dirname(os.path.abspath(__file__))


def safe(s):
    """A file or folder name from device addresses and labels: letters, digits, dot, dash, underscore only."""
    s = re.sub(r"[^A-Za-z0-9_.-]", "_", str(s)).strip("._")[:64]
    return s or "unnamed"


def inside(root, path):
    """Refuse to write anywhere but under the export folder."""
    r, p = os.path.realpath(root), os.path.realpath(path)
    if not (p == r or p.startswith(r + os.sep)):
        raise SystemExit("refusing to write outside %s: %s" % (root, path))
    return path


def iso(t):
    return datetime.fromtimestamp(t, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ") if t else ""


def parse_iso(s):
    return datetime.strptime(s.replace("Z", "+0000"), "%Y-%m-%dT%H:%M:%S.%f%z" if "." in s else "%Y-%m-%dT%H:%M:%S%z").timestamp()


def load_labels(paths):
    spans, by_id = [], {}
    for p in paths or []:
        for r in csv.DictReader(open(p)):
            if "start_utc" in r:
                spans.append((r["device"], parse_iso(r["start_utc"]), parse_iso(r["end_utc"]), r["label"]))
            else:
                by_id[(r.get("device") or r.get("tag_addr"), str(r.get("record_id") or r.get("pull_id")))] = r["label"]
    return spans, by_id


def as_class(lab, classes):
    """Label files may give a class by number (0, 1, ...) or by word."""
    return classes[int(lab)] if lab.isdigit() and int(lab) < len(classes) else lab


def label_at(spans, device, t):
    for d, a, b, lab in spans:
        if d == device and a <= t < b:
            return lab
    return ""


def split_of(key):
    """train or test by a hash of a whole segment or device, never by row, so neighbours do not straddle the split."""
    return "test" if int(hashlib.sha1(key.encode()).hexdigest(), 16) % 5 == 0 else "train"


def write_csv(path, header, rows):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        if header:
            w.writerow(header)
        w.writerows(rows)


def segments(samples, rate_hz):
    """Runs of one device's samples with one label and no gap over three sample periods."""
    out, cur = [], []
    for s in samples:
        if cur and (s["label"] != cur[-1]["label"] or s["t"] - cur[-1]["t"] > 3.0 / rate_hz):
            out.append(cur)
            cur = []
        cur.append(s)
    if cur:
        out.append(cur)
    return out


def export_timeseries(app, samples, out):
    tr = app["training"]
    ch, W, S = tr["channels"], tr["window"], tr["stride"]
    lab_field = app["label"]["field"]
    gold, counts, seen = [], {}, {}
    devices = sorted({s["device"] for s in samples})
    for dev in devices:
        mine = sorted((s for s in samples if s["device"] == dev), key=lambda s: s["t"])
        t0 = mine[0]["t"] if mine else 0
        write_csv(inside(out, os.path.join(out, "capture", safe(dev.replace(":", "")) + ".csv")), ["t_ms"] + ch + ["label"],
                  [[int(1000 * (s["t"] - t0))] + [s["fields"].get(c, "") for c in ch] + [s["label"] or "none"] for s in mine])
        for k, seg in enumerate(segments(mine, tr["rate_hz"])):
            lab = seg[0]["label"]
            if not lab or len(seg) < W:
                continue
            seg_id = "%s-%03d" % (safe(dev.replace(":", "")), k)
            seen[lab] = seen.get(lab, 0) + 1
            split = "test" if seen[lab] % 4 == 0 else "train"     # whole segments; every class gets test segments
            # Tensorlab and Edge AI Studio: one file per labelled segment, the folder is the label; they window it
            write_csv(inside(out, os.path.join(out, "tensorlab", "classes", safe(lab), seg_id + ".csv")), ch,
                      [[s["fields"].get(c, 0) for c in ch] for s in seg])
            counts.setdefault(split, []).append("classes/%s/%s.csv" % (safe(lab), seg_id))
            for w0 in range(0, len(seg) - W + 1, S):
                win = seg[w0:w0 + W]
                feats = {}
                for c in ch:
                    v = [int(s["fields"].get(c, 0)) for s in win]
                    feats[c + "_mean"] = round(statistics.mean(v))
                    feats[c + "_min"], feats[c + "_max"] = min(v), max(v)
                own = statistics.mode([s["fields"].get(lab_field, "") for s in win])
                gold.append(dict(window=seg_id + "-%03d" % w0, device=dev, start_utc=iso(win[0]["t"]), label=lab,
                                 device_said=own, split=split, **feats))
                # Edge Impulse: one CSV per window, a timestamp column in ms, the label in the file name
                write_csv(inside(out, os.path.join(out, "edge-impulse", "%s.%s-%03d.csv" % (safe(lab), seg_id, w0))), ["timestamp"] + ch,
                          [[int(1000 * (s["t"] - win[0]["t"]))] + [s["fields"].get(c, 0) for c in ch] for s in win])
    ann = os.path.join(out, "tensorlab", "annotations")
    os.makedirs(ann, exist_ok=True)
    for split, name in (("train", "train"), ("test", "val"), ("test", "test")):
        open(os.path.join(ann, "instances_%s_list.txt" % name), "w").write("\n".join(counts.get(split, [])) + "\n")
    return gold


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=os.path.abspath("out/tool-telemetry/homebase.sqlite"))
    ap.add_argument("--labels", action="append", default=[])
    ap.add_argument("--out", default=os.path.abspath("out/fielddata-export"))
    ap.add_argument("--license", default="CC-BY-4.0")
    ap.add_argument("--creator", default="Fox Red Labs")
    ap.add_argument("--url", default="https://example.invalid/field-data", help="where the dataset will live")
    ap.add_argument("--version", default="0.1.0")
    ap.add_argument("--provenance", default="", help="one sentence on where the data came from, such as 'simulated devices'")
    a = ap.parse_args()
    today = time.strftime("%Y-%m-%d", time.gmtime())

    def meta(app):
        return dict(license=a.license, creator=a.creator, url=a.url, version=a.version, date=today,
                    description=" ".join(x for x in (app["about"], a.provenance) if x))
    apps = fd.load_apps()
    db = sqlite3.connect(a.db)
    spans, by_id = load_labels(a.labels)
    names = dict(db.execute("select addr, coalesce(tool, name) from tags").fetchall())
    os.makedirs(a.out, exist_ok=True)
    env = open(os.path.join(a.out, "records.jsonl"), "w")
    summary = []

    # generic apps: records table
    for app_id, app in apps.items():
        if app.get("wire") == "fixed":
            continue
        rows = db.execute("select hub, a, ident, unix, received, via, fields from records where app_id=? order by a, unix",
                          (app_id,)).fetchall()
        if not rows:
            continue
        samples, envs = [], []
        for hub, dev, ident, unix, received, via, fields in rows:
            f = fd.named(app, {int(k): v for k, v in json.loads(fields).items()})
            lab = by_id.get((dev, ident)) or label_at(spans, dev, unix)
            samples.append(dict(device=dev, t=unix, fields=f, label=lab))
            envs.append(fd.envelope(app, dev, hub, app["record_kind"], ident, iso(unix), iso(received), via, f, lab or None))
            env.write(json.dumps(envs[-1]) + "\n")
        od = inside(a.out, os.path.join(a.out, safe(app["name"])))
        cols = [f["name"] for f in app["fields"]]
        write_csv(os.path.join(od, "samples.csv"), ["device", "device_name", "event_time_utc", "label"] + cols,
                  [[s["device"], names.get(s["device"], ""), iso(s["t"]), s["label"]] + [s["fields"].get(c, "") for c in cols]
                   for s in samples])
        gold = export_timeseries(app, samples, od) if app["training"]["task"].startswith("timeseries") else []
        if gold:
            write_csv(os.path.join(od, "gold.csv"), list(gold[0]), [list(g.values()) for g in gold])
        cards.describe(od, app, envs, meta(app))
        summary.append("%s: %d records from %d devices, %d labelled, %d training windows" % (
            app["name"], len(samples), len({s["device"] for s in samples}), sum(1 for s in samples if s["label"]), len(gold)))

    # tool telemetry: its pulls are records too (fixed wire format, same envelope)
    tool = next(x for x in apps.values() if x["name"] == "tool-telemetry")
    pulls = db.execute("select hub, a, pid, unix, received, via, dur_ms, rpm_avg, rpm_max, i_avg_ma, i_peak_ma, v_min_mv, "
                       "temp_c, model_cls from pulls order by a, unix").fetchall()
    if pulls:
        od = inside(a.out, os.path.join(a.out, safe(tool["name"])))
        ch = tool["training"]["channels"]
        gold, envs, flat = [], [], []
        ntools = len({p[1] for p in pulls})          # with few tools, split by pull (pulls do not overlap like windows do)
        for hub, dev, pid, unix, received, via, *vals, cls in pulls:
            f = dict(zip(ch, vals))
            f["load"] = tool["label"]["classes"][cls] if cls is not None and cls < 4 else ""
            lab = as_class(by_id.get((dev, str(pid))) or label_at(spans, dev, unix), tool["label"]["classes"])
            envs.append(fd.envelope(tool, dev, hub, "pull", str(pid), iso(unix), iso(received), via, f, lab or None))
            env.write(json.dumps(envs[-1]) + "\n")
            flat.append([dev, names.get(dev, ""), iso(unix), lab] + [f.get(c, "") for c in cols])
            key = "%s-%s" % (safe(dev.replace(":", "")), safe(pid))
            gold.append(dict(record=key, device=dev, event_time_utc=iso(unix), label=lab, device_said=f["load"],
                             split=split_of(names.get(dev, dev) if ntools >= 5 else key), **{c: f[c] for c in ch}))
            if lab:
                write_csv(inside(a.out, os.path.join(od, "edge-impulse", "%s.%s.csv" % (safe(lab), key))), ch, [[f[c] for c in ch]])
        write_csv(os.path.join(od, "gold.csv"), list(gold[0]), [list(g.values()) for g in gold])
        write_csv(os.path.join(od, "samples.csv"), ["device", "device_name", "event_time_utc", "label"] + cols, flat)
        # Tensorlab has no tabular flow (research/ti-edge-ai-dataset-formats-2026-10-17.md): time-ordered rows per class
        for lab in tool["label"]["classes"]:
            rows = [[g[c] for c in ch] for g in gold if g["label"] == lab]
            if rows:
                write_csv(inside(a.out, os.path.join(od, "tensorlab", "classes", safe(lab), "pulls.csv")), ch, rows)
        cards.describe(od, tool, envs, meta(tool))
        summary.append("tool-telemetry: %d pulls, %d labelled" % (len(gold), sum(1 for g in gold if g["label"])))
    env.close()
    json.dump(list(apps.values()), open(os.path.join(a.out, "schema.json"), "w"), indent=1)
    sheet = open(os.path.join(HERE, "DATASHEET-template.md")).read()
    open(os.path.join(a.out, "DATASHEET.md"), "w").write(sheet.replace("{{exported}}", time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime()))
                                                       .replace("{{summary}}", "\n".join("- " + s for s in summary)))
    print("\n".join(summary) + "\nexported to " + a.out)


if __name__ == "__main__":
    main()
