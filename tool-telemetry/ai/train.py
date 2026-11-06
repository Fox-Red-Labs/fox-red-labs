#!/usr/bin/env python3
"""Train the pull-load classifier and write it as C for the tag (or any MCU). Standard library only.
    python3 tool-telemetry/ai/train.py [--from-gold path/to/pulls.csv] [--seed 1]
Without --from-gold it trains on simulated pulls from ai/physics.py (30 tools, 200 pulls each); with it, on real pulls
from the dataset's gold table (needs a "label" column). Splits by tool, never by row, so the test score is for tools the
model has not seen. Writes ai/out/tt_model.c and tt_model.h (two trees: one using current, one speed-only for tools that
cannot measure current), ai/out/model.json, and ai/MODEL.md (the model card with the scores)."""
import argparse
import csv
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import physics  # noqa: E402

# feature name in the dataset -> field in tt_pull_t and how the firmware stores it
FIELDS = {"dur10": ("dur_ms", 0.1), "ravg": ("rpm_avg", 1), "rmax": ("rpm_max", 1), "iavg": ("i_avg_ma", 1),
          "ipk": ("i_peak_ma", 1), "vmin": ("v_min_mv", 1), "tc": ("temp_c", 1)}
FULL = ("dur10", "ravg", "rmax", "iavg", "ipk", "vmin", "tc")
SPEED_ONLY = ("dur10", "ravg", "rmax")


def simulate(seed, tools=30, per_tool=200):
    rows = []
    for t in range(tools):
        r = random.Random(seed * 1000 + t)
        profile = ("mixed", "finish", "framing")[t % 3]
        pack = r.uniform(18800, 20600)                      # pack age and charge differ by tool
        temp = 24.0
        for _ in range(per_tool):
            cls = physics.task_mix(r, profile)
            f, temp = physics.pull(r, cls, int(pack), temp)
            temp = physics.cool(temp, r.uniform(2, 40))
            pack -= r.uniform(0, 4)
            rows.append(dict(tool="sim-%02d" % t, label=cls, **f))
    return rows


def to_fw(row):
    """The integer values the firmware sees (tt_pull_t)."""
    x = {k: int(round(float(row[src]) * scale)) for k, (src, scale) in FIELDS.items()}
    x["tc"] += 128                      # temperature is signed; the trees compare unsigned values, so offset it
    return x


def gini(counts):
    n = sum(counts)
    return 1.0 - sum((c / n) ** 2 for c in counts) if n else 0.0


def build(X, y, feats, depth, max_depth, min_leaf, nodes):
    counts = [y.count(c) for c in range(4)]
    majority = max(range(4), key=lambda c: counts[c])
    if depth >= max_depth or len(y) < 2 * min_leaf or max(counts) == len(y):
        nodes.append(dict(leaf=majority, n=len(y), counts=counts))
        return len(nodes) - 1
    best = None
    base = gini(counts)
    for f in feats:
        order = sorted(range(len(y)), key=lambda i: X[i][f])
        left = [0] * 4
        right = counts[:]
        for k in range(len(order) - 1):
            i = order[k]
            left[y[i]] += 1
            right[y[i]] -= 1
            a, b = X[i][f], X[order[k + 1]][f]
            if a == b or k + 1 < min_leaf or len(y) - k - 1 < min_leaf:
                continue
            nl = k + 1
            g = (nl * gini(left) + (len(y) - nl) * gini(right)) / len(y)
            if best is None or g < best[0]:
                best = (g, f, (a + b) // 2)
    if best is None or base - best[0] < 1e-4:
        nodes.append(dict(leaf=majority, n=len(y), counts=counts))
        return len(nodes) - 1
    _, f, thr = best
    me = len(nodes)
    nodes.append(dict(f=f, thr=thr))
    li = [i for i in range(len(y)) if X[i][f] <= thr]
    ri = [i for i in range(len(y)) if X[i][f] > thr]
    nodes[me]["l"] = build([X[i] for i in li], [y[i] for i in li], feats, depth + 1, max_depth, min_leaf, nodes)
    nodes[me]["r"] = build([X[i] for i in ri], [y[i] for i in ri], feats, depth + 1, max_depth, min_leaf, nodes)
    return me


def predict(nodes, x):
    n = nodes[0]
    while "leaf" not in n:
        n = nodes[n["l"]] if x[n["f"]] <= n["thr"] else nodes[n["r"]]
    return n["leaf"]


def score(nodes, X, y):
    cm = [[0] * 4 for _ in range(4)]
    for x, t in zip(X, y):
        cm[t][predict(nodes, x)] += 1
    acc = sum(cm[i][i] for i in range(4)) / max(1, len(y))
    recall = [cm[i][i] / max(1, sum(cm[i])) for i in range(4)]
    return acc, recall, cm


def c_tree(name, nodes):
    lines = ["static const tt_node_t %s[%d] = {" % (name, len(nodes))]
    for n in nodes:
        if "leaf" in n:
            lines.append("    { -1, %d, 0, 0 },   /* class %s, %d training pulls */" % (n["leaf"], physics.CLASSES[n["leaf"]], n["n"]))
        else:
            lines.append("    { %d, %d, %d, %d },   /* %s <= %d */" % (FULL.index(n["f"]), n["thr"], n["l"], n["r"], n["f"], n["thr"]))
    lines.append("};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from-gold")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--depth", type=int, default=5)
    ap.add_argument("--out", help="output folder (default ai/out, or ai/out-collected with --from-gold so the shipped model is not replaced)")
    a = ap.parse_args()
    if a.from_gold:
        rows = [r for r in csv.DictReader(open(a.from_gold)) if r.get("label") not in (None, "")]
        for r in rows:
            r["label"] = physics.CLASSES.index(r["label"]) if r["label"] in physics.CLASSES else int(r["label"])
            r["tool"] = r.get("tool_id") or r.get("tool")
            r["i_avg_ma"] = r["i_avg_ma"] or 0
            r["i_peak_ma"] = r["i_peak_ma"] or 0
        kinds = {r.get("label_source") for r in rows}
        source = "%d labelled pulls from %s (labels: %s)" % (len(rows), os.path.basename(a.from_gold), ", ".join(sorted(k for k in kinds if k)))
    else:
        rows = simulate(a.seed)
        source = "simulated pulls (ai/physics.py, seed %d)" % a.seed
    split_note = "by tool"
    tools = sorted({r["tool"] for r in rows})
    if len(tools) >= 3:
        test_tools = set(tools[int(len(tools) * 0.8):])
        tr = [r for r in rows if r["tool"] not in test_tools]
        te = [r for r in rows if r["tool"] in test_tools]
        split_note = "by tool"
    else:                                   # too few tools to hold one out: hold out the last 20% in time instead
        cut = int(len(rows) * 0.8)
        tr, te, test_tools = rows[:cut], rows[cut:], set()
        split_note = "by time (only %d tool%s: the score is optimistic for a new tool)" % (len(tools), "" if len(tools) == 1 else "s")
    Xtr, ytr = [to_fw(r) for r in tr], [r["label"] for r in tr]
    Xte, yte = [to_fw(r) for r in te], [r["label"] for r in te]
    out = a.out or os.path.join(HERE, "out" if not a.from_gold else "out-collected")
    os.makedirs(out, exist_ok=True)
    models, report = {}, {}
    for name, feats in (("full", FULL), ("speed", SPEED_ONLY)):
        nodes = []
        build(Xtr, ytr, feats, 0, a.depth, 8, nodes)
        models[name] = nodes
        acc, rec, cm = score(nodes, Xte, yte)
        report[name] = dict(features=feats, nodes=len(nodes), test_accuracy=round(acc, 3),
                            recall={physics.CLASSES[i]: round(rec[i], 3) for i in range(4)}, confusion=cm)
    json.dump(dict(source=source, split=split_note if a.from_gold else "by tool", train_tools=len(tools) - len(test_tools), test_tools=sorted(test_tools),
                   train_pulls=len(tr), test_pulls=len(te), models=models, report=report),
              open(os.path.join(out, "model.json"), "w"), indent=1)

    h = """/* Generated by tool-telemetry/ai/train.py. Do not edit. Trained on %s. */
#ifndef TT_MODEL_H
#define TT_MODEL_H
#include "tt_proto.h"
/* Load class of one trigger pull: 0 light, 1 normal, 2 heavy, 3 jam. Uses the speed-only tree when the tool reported
   no current (iavg and ipk both 0). */
uint8_t tt_model_classify(const tt_pull_t *p);
#endif
""" % source
    c = """/* Generated by tool-telemetry/ai/train.py. Do not edit. Trained on %s.
   Test on %d pulls from %d tools it never saw: %.1f%% with current, %.1f%% speed only. Model card: ai/MODEL.md. */
#include "tt_model.h"

typedef struct { int8_t f; uint16_t thr; uint8_t l, r; } tt_node_t;   /* f < 0: leaf, thr is the class */

%s

%s

static uint16_t feat(const tt_pull_t *p, int f)
{
    switch (f) {
    case 0: return p->dur10;  case 1: return p->ravg;  case 2: return p->rmax;  case 3: return p->iavg;
    case 4: return p->ipk;    case 5: return p->vmin;  default: return (uint16_t)(p->tc + 128);
    }
}

static uint8_t walk(const tt_node_t *t, const tt_pull_t *p)
{
    const tt_node_t *n = t;
    while (n->f >= 0) n = (feat(p, n->f) <= n->thr) ? &t[n->l] : &t[n->r];
    return (uint8_t)n->thr;
}

uint8_t tt_model_classify(const tt_pull_t *p)
{
    return walk((p->iavg == 0 && p->ipk == 0) ? TREE_SPEED : TREE_FULL, p);
}
""" % (source, len(te), len(test_tools), 100 * report["full"]["test_accuracy"], 100 * report["speed"]["test_accuracy"],
       c_tree("TREE_FULL", models["full"]), c_tree("TREE_SPEED", models["speed"]))
    open(os.path.join(out, "tt_model.h"), "w").write(h)
    open(os.path.join(out, "tt_model.c"), "w").write(c)
    card = ["# Model card: pull load classifier", "",
            "Generated by `ai/train.py`. One decision per trigger pull: light, normal, heavy or jam. Runs on the tag "
            "(`tt_model_classify`, called when a pull summary arrives without a class) and on any MCU that compiles `tt_model.c`.", "",
            "| | |", "|---|---|", "| Trained on | %s |" % source, "| Split | %s; %d training pulls, %d test pulls |" % (
                split_note if a.from_gold else "by tool (%d tools train, %d test)" % (len(tools) - len(test_tools), len(test_tools)), len(tr), len(te)),
            "| Inputs | pull length, average and peak speed, average and peak current, lowest supply voltage, temperature |",
            "| Size | %d nodes (with current), %d nodes (speed only); a node is 6 bytes of flash |" % (report["full"]["nodes"], report["speed"]["nodes"]),
            "| Depth | %d |" % a.depth, ""]
    for name, title in (("full", "With current"), ("speed", "Speed only (the tool reports no current)")):
        r = report[name]
        card += ["## %s: %.1f%% on the test pulls" % (title, 100 * r["test_accuracy"]), "",
                 "| true \\ predicted | " + " | ".join(physics.CLASSES) + " | recall |", "|---" * 6 + "|"]
        for i, row in enumerate(r["confusion"]):
            card.append("| %s | %s | %.0f%% |" % (physics.CLASSES[i], " | ".join(str(v) for v in row), 100 * r["recall"][physics.CLASSES[i]]))
        card.append("")
    card += ["## Read this before trusting it", "",
             "- Simulated pulls (`ai/physics.py`) make speed and current both follow the load, so the speed-only tree does as well as "
             "the full one. Real motors will not be this tidy; retrain on real pulls (`--from-gold`) before drawing conclusions.",
             "- The tool's own overcurrent trip is the safety path. This model reports; it never stops a motor.",
             "- Score a new model only on tools it never saw. A time split on one tool (what `--from-gold` falls back to) is optimistic.", ""]
    if not a.from_gold:
        open(os.path.join(HERE, "MODEL.md"), "w").write("\n".join(card))
    else:
        open(os.path.join(out, "MODEL.md"), "w").write("\n".join(card))
    for name in ("full", "speed"):
        r = report[name]
        print("%-6s %2d nodes  test accuracy %.1f%%  recall %s" % (name, r["nodes"], 100 * r["test_accuracy"],
                                                              " ".join("%s %.0f%%" % (k, 100 * v) for k, v in r["recall"].items())))
    print("split:", split_note if a.from_gold else "by tool", "| train", len(tr), "test", len(te))
    print("wrote", out)


if __name__ == "__main__":
    main()
