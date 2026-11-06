#!/usr/bin/env python3
"""Turn what the home base collected into a dataset a data team can train on. Standard library only.
    python3 tool-telemetry/ai/dataset.py [--db homebase.sqlite] [--raw homebase-raw/] [--out dataset/] [--truth truth.csv]
Three layers, the usual "medallion" layout:
  bronze/  every upload exactly as it arrived (copied from the home base's raw folder; never edited)
  silver/  one clean table per record type: typed columns, units in the names, UTC times, where it came from, deduplicated
  gold/    pulls.csv: one row per trigger pull with its features, context and (when known) a label, ready to train on
plus schema.json (every column, its type, unit and meaning) and DATASHEET.md (what is in it, how it was collected, what it
is for and not for). CSV, so it opens anywhere; load it into Parquet, a warehouse or a notebook with one line.
--truth adds labels from a file of (tag address, pull id, label) rows, for example the simulator's ground truth."""
import argparse
import csv
import hashlib
import json
import os
import shutil
import sqlite3
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CLASSES = ("light", "normal", "heavy", "jam")
EVENT_NAMES = {1: "trigger_pull", 2: "overcurrent_trip", 3: "driver_fault", 4: "tool_restart", 5: "tag_restart"}

SCHEMA = {
    "silver/snapshots.csv": [
        ("hub_id", "string", "", "the hub that heard the tag"), ("tag_addr", "string", "", "the tag's Bluetooth address"),
        ("tool_id", "string", "", "the tool's name, as its MCU reports it"),
        ("event_time_utc", "timestamp", "", "when the snapshot was logged by the hub (ISO 8601, UTC)"),
        ("ingest_time_utc", "timestamp", "", "when the home base received it"), ("source", "string", "", "phone, modem or wifi"),
        ("seq", "int", "", "the tag's snapshot counter (wraps at 256)"), ("state", "string", "", "idle, running, fault, locked"),
        ("trigger_pulls_total", "int", "count", "since the tool MCU last restarted"),
        ("overcurrent_trips_total", "int", "count", ""), ("driver_faults_total", "int", "count", ""),
        ("motor_run_s_total", "int", "s", ""), ("rpm", "int", "rpm", "speed at the snapshot"),
        ("battery_soc_pct", "int", "%", "empty when no battery data"), ("tool_temp_c", "int", "deg C", "")],
    "silver/events.csv": [
        ("hub_id", "string", "", ""), ("tag_addr", "string", "", ""), ("tool_id", "string", "", ""),
        ("event_time_utc", "timestamp", "", "when it happened, from the tag's clock and the hub's; empty if before a tag restart"),
        ("ingest_time_utc", "timestamp", "", ""), ("source", "string", "", ""),
        ("event_id", "int", "", "the tag's running event number"), ("event", "string", "", "trigger_pull, overcurrent_trip, ..."),
        ("value", "int", "", "trigger_pull: length in ms; overcurrent_trip: peak current in mA (0 = not measured)")],
    "silver/pulls.csv": [
        ("hub_id", "string", "", ""), ("tag_addr", "string", "", ""), ("tool_id", "string", "", ""),
        ("pull_id", "int", "", "the tag's running pull number"), ("event_time_utc", "timestamp", "", "when the hub logged it"),
        ("ingest_time_utc", "timestamp", "", ""), ("source", "string", "", ""),
        ("dur_ms", "int", "ms", "trigger held"), ("rpm_avg", "int", "rpm", ""), ("rpm_max", "int", "rpm", ""),
        ("i_avg_ma", "int", "mA", "0 = the tool cannot measure current"), ("i_peak_ma", "int", "mA", ""),
        ("v_min_mv", "int", "mV", "lowest supply during the pull"), ("temp_c", "int", "deg C", "tool temperature at the end"),
        ("model_class", "string", "", "what the model on the tag called it: light, normal, heavy, jam")],
    "gold/pulls.csv": [
        ("pull_key", "string", "", "hub_id:tag_addr:pull_id, unique"), ("tool_id", "string", "", ""),
        ("event_time_utc", "timestamp", "", ""), ("hour_local", "int", "h", "hour of day, for shift patterns"),
        ("dur_ms", "int", "ms", ""), ("rpm_avg", "int", "rpm", ""), ("rpm_max", "int", "rpm", ""),
        ("rpm_sag_pct", "float", "%", "how far speed fell below the pull's peak: 100 x (rpm_max - rpm_avg) / rpm_max"),
        ("i_avg_ma", "int", "mA", ""), ("i_peak_ma", "int", "mA", ""), ("v_min_mv", "int", "mV", ""), ("temp_c", "int", "deg C", ""),
        ("energy_j_est", "float", "J", "v_min x i_avg x dur: a lower bound, empty when current is not measured"),
        ("battery_soc_pct", "int", "%", "from the nearest snapshot before the pull, empty if none"),
        ("model_class", "string", "", "the tag's on-device prediction"),
        ("label", "string", "", "the true class when known (simulator ground truth or a person's tag), else empty"),
        ("label_source", "string", "", "simulator, human, or empty"),
        ("split", "string", "", "train or test, by a hash of tool_id so a tool's pulls never straddle the split")],
}


def iso(t):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(t)) if t else ""


def write(path, cols, rows):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow([c[0] for c in cols])
        w.writerows(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=os.path.abspath("out/tool-telemetry/homebase.sqlite"))
    ap.add_argument("--raw", default=os.path.abspath("out/tool-telemetry/homebase-raw"))
    ap.add_argument("--out", default=os.path.abspath("out/tool-telemetry/dataset"))
    ap.add_argument("--truth", help="CSV with tag_addr,pull_id,label (label a class name or 0 to 3)")
    a = ap.parse_args()
    db = sqlite3.connect(a.db)
    tools = {r[0]: (r[1] or r[2] or r[0]) for r in db.execute("select addr, tool, name from tags")}
    if os.path.isdir(a.raw):
        shutil.copytree(a.raw, os.path.join(a.out, "bronze"), dirs_exist_ok=True)
    st_name = ("idle", "running", "fault", "locked")

    snaps = db.execute("select hub, a, unix, received, via, seq, st, trig, oc, flt, run, rpm, soc, tc from items "
                       "where k='s' order by a, unix").fetchall()
    write(os.path.join(a.out, "silver/snapshots.csv"), SCHEMA["silver/snapshots.csv"],
          [(h, ad, tools.get(ad, ""), iso(u), iso(rc), via, seq, st_name[st or 0] if st is not None else "", trig, oc, flt, run,
            rpm, "" if soc in (None, 255) else soc, tc) for h, ad, u, rc, via, seq, st, trig, oc, flt, run, rpm, soc, tc in snaps])
    evs = db.execute("select hub, a, unix, received, via, ident, ev_type, ev_v from items where k='e' order by a, unix").fetchall()
    write(os.path.join(a.out, "silver/events.csv"), SCHEMA["silver/events.csv"],
          [(h, ad, tools.get(ad, ""), iso(u), iso(rc), via, ident.split(":")[1], EVENT_NAMES.get(ty, str(ty)), v)
           for h, ad, u, rc, via, ident, ty, v in evs])
    pulls = db.execute("select hub, a, pid, unix, received, via, dur_ms, rpm_avg, rpm_max, i_avg_ma, i_peak_ma, v_min_mv, "
                       "temp_c, model_cls from pulls order by a, unix, pid").fetchall()
    write(os.path.join(a.out, "silver/pulls.csv"), SCHEMA["silver/pulls.csv"],
          [(h, ad, tools.get(ad, ""), pid, iso(u), iso(rc), via, d, ra, rm, ia, ip, vm, tc,
            CLASSES[c] if c is not None and c < 4 else "") for h, ad, pid, u, rc, via, d, ra, rm, ia, ip, vm, tc, c in pulls])

    truth = {}
    if a.truth:
        for r in csv.DictReader(open(a.truth)):
            lab = r["label"]
            truth[(r["tag_addr"], int(r["pull_id"]))] = CLASSES[int(lab)] if lab.isdigit() else lab
    soc_by_tag = {}
    for h, ad, u, rc, via, seq, st, trig, oc, flt, run, rpm, soc, tc in snaps:
        if soc not in (None, 255) and u:
            soc_by_tag.setdefault(ad, []).append((u, soc))
    gold = []
    for h, ad, pid, u, rc, via, d, ra, rm, ia, ip, vm, tc, c in pulls:
        tool = tools.get(ad, ad)
        socs = [s for t, s in soc_by_tag.get(ad, []) if t <= u]
        lab = truth.get((ad, pid), "")
        split = "test" if int(hashlib.sha1(tool.encode()).hexdigest(), 16) % 5 == 0 else "train"
        gold.append(("%s:%s:%s" % (h, ad, pid), tool, iso(u), time.localtime(u).tm_hour if u else "", d, ra, rm,
                     round(100.0 * (rm - ra) / rm, 1) if rm else "", ia, ip, vm, tc,
                     round(vm / 1000 * ia / 1000 * d / 1000, 1) if ia else "", socs[-1] if socs else "",
                     CLASSES[c] if c is not None and c < 4 else "", lab, ("simulator" if lab else ""), split))
    write(os.path.join(a.out, "gold/pulls.csv"), SCHEMA["gold/pulls.csv"], gold)

    json.dump({k: [dict(name=n, type=t, unit=u, meaning=m) for n, t, u, m in v] for k, v in SCHEMA.items()},
              open(os.path.join(a.out, "schema.json"), "w"), indent=1)
    shutil.copy(os.path.join(HERE, "DATASHEET.md"), os.path.join(a.out, "DATASHEET.md"))
    labelled = sum(1 for g in gold if g[15])
    print("dataset in %s: %d snapshots, %d events, %d pulls (%d labelled), %d tools" % (
        a.out, len(snaps), len(evs), len(pulls), labelled, len({g[1] for g in gold})))


if __name__ == "__main__":
    main()
