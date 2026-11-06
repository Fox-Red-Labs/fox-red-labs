#!/usr/bin/env python3
"""Every example against every TI board on the bench: a verdict and a named blocker per cell, before
anything is built. Reads data/inventory.jsonl (what each example needs) and profiles.json (what each board
has and which layers and bridges exist). Writes data/map.jsonl (one line per example-to-board cell),
data/summary.json and SUMMARY.md (the matrix, the blocker ranking, the queue of cells worth building).

Verdict words are the lane's six (portability/README.md). Rules, in the order they fire:
  1. Does not make sense   the example needs a radio, Wi-Fi, USB, a display, Ethernet, a second core or a
                           PRU the target does not have
  2. Does not exist        the example's driver layer is absent on the target and no shim bridges it
                           (every Arm layer on C2000, C2000 driverlib on Arm, TI Drivers on MSPM33, ...)
  3. Could work            a block the example uses is missing on the target, or has a different API there
                           (partial bridge), or FreeRTOS only fits on paper, or the native image is bigger
                           than the target's flash or RAM
  4. Should work           same SDK (retarget in SysConfig) or a shim already covers the layer, and every
                           block the example needs is on the target
Each cell also says whether the bridge it relies on is measured (a matrix.csv row), inferred, or untried.

Run:  python3 feasibility.py
"""
import json, os, collections
HERE = os.path.dirname(os.path.abspath(__file__))
V = json.load(open(os.path.join(HERE, "vocab.json")))
P = json.load(open(os.path.join(HERE, "profiles.json")))
HW = set(V["hardware_tags"])
NO_SENSE = {"RADIO_BLE", "RADIO_ZIGBEE", "RADIO_THREAD", "RADIO_RF", "WIFI", "USB", "DISPLAY", "ETHERNET", "MULTICORE", "PRU", "SDFM", "EPI", "RADAR", "RADAR_HWA"}
CPU_FALLBACK = {"NPU": "no NPU on the target: the model runs on the CPU (measured elsewhere in this repo)",
                "TMU": "no TMU: the maths runs in software", "MATHACL": "no MATHACL: IQmath falls back to the RTS library (X21)"}
DRIVER_LAYERS = ["tidrivers", "driverlib_msp", "am13_driverlib", "mcuplus_drivers", "c2000_driverlib", "stm32_hal"]
FAMILY_OF_BOARD = {b["sdk_board"]: b["family"] for b in P["boards"].values()}
FAMILY_OF_BOARD.update({"LP_EM_CC2340R53": "F3", "LP_EM_CC2745R10_Q1": "F3", "LP_EM_CC2755P20": "F3", "LP_EM_CC2755R10_BG": "F3", "CC2745R10_CS_EVM": "F3",
                        "LP_EM_CC35X1ET": "WIFI"})

def primary_layer(ex):
    mods = ex["modules"]
    if any(m.startswith("/ti/drivers/") or m.startswith("/ti/display/") for m in mods):
        return "tidrivers"
    for l in DRIVER_LAYERS:
        if l in ex["layers"]:
            return l
    return "plain_c"

def strong_needs(ex):
    """Hardware tags backed by a SysConfig module or an example path, not only by a header or call name."""
    tags = set()
    for m in ex["modules"]:
        t = V["modules"].get(m)
        if t in HW:
            tags.add(t)
    low = "/" + ex["path"].lower() + "/"
    for frag, t in V["paths"].items():
        if frag.lower() in low and t in HW:
            tags.add(t)
    if not ex["modules"]:                       # no SysConfig file: fall back to what the sources say
        tags |= set(ex["hw_tags"])
    return tags

def target_caps(bid, board, inv):
    if "hw_from_sdk_override" in board:
        caps = set(board["hw_from_sdk_override"])
    else:
        caps = set()
        for ex in inv:
            if ex["board"] == board["sdk_board"]:
                for m in ex["modules"]:
                    t = V["modules"].get(m)
                    if t in HW:
                        caps.add(t)
    caps |= set(board.get("hw_extra", {}))
    caps -= set(board.get("hw_missing", []))
    return caps

def bridge(layer, ffam, tfam):
    table = P["bridges"].get(layer, {})
    return table.get(f"{ffam}->{tfam}") or table.get("*")

def judge(ex, bid, board, caps):
    ffam, tfam = ex["family"], board["family"]
    layer = primary_layer(ex)
    needs = strong_needs(ex)
    blockers, notes = [], []
    # 1. hardware the target cannot stand in for
    hard = sorted(t for t in needs & NO_SENSE if t not in caps)
    if hard:
        return dict(verdict="Does not make sense", blockers=[f"target has no {t}" for t in hard], layer=layer, bridge=None, evidence="n/a")
    # 2. the layer
    br = None
    if layer == "plain_c":
        kind, evidence = "plain_c", "Plain C moves with the compiler (X5, X19)"
    else:
        avail = board["layers"].get(layer)
        br = bridge(layer, ffam, tfam)
        if ffam == tfam and avail in ("native", "partial", "size"):
            br = br or dict(kind="same_sdk", evidence="same SDK")
        if br is None or br.get("kind") in ("none", "rewrite") or (avail is None and br.get("kind") not in ("shim", "partial")):
            return dict(verdict="Does not exist", blockers=[f"{layer} is not on the {board['name']} and no shim bridges {ffam} to {tfam}"], layer=layer, bridge=None, evidence="n/a")
        kind, evidence = br["kind"], br.get("evidence", "")
        if kind == "partial":
            shared = set(br.get("shared", []))
            diff = sorted(t for t in needs if t in caps and t not in shared)
            for t in diff:
                blockers.append(f"{t} has a different API on the {board['name']} ({layer} names shared only for {', '.join(sorted(shared))})")
    # 3. missing blocks, size, RTOS
    for t in sorted(needs - caps):
        if t in CPU_FALLBACK:
            blockers.append(CPU_FALLBACK[t])
        else:
            blockers.append(f"target has no {t}")
    if "rtos_freertos" in ex["layers"]:
        r = board["layers"].get("rtos_freertos")
        if r is None:
            return dict(verdict="Does not exist", blockers=["FreeRTOS is not in this board's SDK on this Mac"], layer=layer, bridge=kind, evidence=evidence)
        if r == "size":
            blockers.append(f"FreeRTOS fits only on paper in {board['ram_kb']} KB of RAM")
    if layer == "tidrivers" and board["layers"].get("tidrivers") == "size":
        blockers.append(f"TI Drivers images do not fit {board['ram_kb']} KB of RAM (experiment 2026-10-08-mspm0c1104-1kb-ram)")
    ns = ex.get("native_size")
    if ns:
        flash, ram = ns["text"] + ns["data"], ns["data"] + ns["bss"]
        if flash > board["flash_kb"] * 1024 * 0.9:
            blockers.append(f"native image {flash:,} B of flash against {board['flash_kb']} KB")
        if ram > board["ram_kb"] * 1024 * 0.9:
            blockers.append(f"native image {ram:,} B of RAM against {board['ram_kb']} KB")
    verdict = "Could work" if blockers else "Should work"
    ev = "measured" if ("X" in evidence and "none" not in evidence) else ("untried" if evidence.startswith("none") or not evidence else "inferred")
    return dict(verdict=verdict, blockers=blockers, layer=layer, bridge=kind, evidence=ev, evidence_note=evidence)

def main():
    inv = [json.loads(l) for l in open(os.path.join(HERE, "data", "inventory.jsonl"))]
    caps = {bid: target_caps(bid, b, inv) for bid, b in P["boards"].items()}
    out = open(os.path.join(HERE, "data", "map.jsonl"), "w")
    cells = collections.Counter()           # (source board, target id, verdict)
    blockers = collections.Counter()        # (target id, blocker)
    queue = collections.Counter()           # (source board, target id, layer, evidence) -> count of Should work
    total = 0
    for ex in inv:
        for bid, b in P["boards"].items():
            if ex["board"] == b["sdk_board"]:
                continue
            j = judge(ex, bid, b, caps[bid])
            row = dict(example=ex["id"], source=ex["board"], target=bid, **j)
            out.write(json.dumps(row) + "\n")
            cells[(ex["board"], bid, j["verdict"])] += 1
            for bl in j["blockers"]:
                blockers[(bid, bl.split(" (")[0])] += 1
            if j["verdict"] == "Should work":
                queue[(ex["board"], bid, j["layer"], j["evidence"])] += 1
            total += 1
    out.close()
    summary = dict(cells=total, examples=len(inv), targets=list(P["boards"]),
                   caps={k: sorted(v) for k, v in caps.items()},
                   matrix={f"{s}->{t}": {} for (s, t, v) in cells})
    for (s, t, v), n in cells.items():
        summary["matrix"][f"{s}->{t}"][v] = n
    summary["blockers"] = [dict(target=t, blocker=b, n=n) for (t, b), n in blockers.most_common()]
    summary["queue"] = [dict(source=s, target=t, layer=l, evidence=e, n=n) for (s, t, l, e), n in queue.most_common()]
    json.dump(summary, open(os.path.join(HERE, "data", "summary.json"), "w"), indent=1)
    write_md(summary, inv)

def write_md(S, inv):
    boards = P["boards"]
    srcs = sorted({ex["board"] for ex in inv}, key=lambda b: (FAMILY_OF_BOARD.get(b, "zz"), b))
    L = ["# Every example on every board: the map on paper", "",
         f"Generated by `feasibility.py` from `data/inventory.jsonl` ({S['examples']:,} examples in 7 SDKs) and `profiles.json` ({len(boards)} target boards): {S['cells']:,} cells. A verdict here is a prediction from what the example asks for and what the board has; the cross-builds and runs in `data/crossbuild/` turn predictions into results. Verdict words: `portability/README.md`.", "",
         "## Totals", ""]
    tot = collections.Counter()
    for k, d in S["matrix"].items():
        for v, n in d.items():
            tot[v] += n
    for v in ["Should work", "Could work", "Does not exist", "Does not make sense"]:
        L.append(f"- **{v}**: {tot[v]:,} cells ({100*tot[v]/S['cells']:.0f} percent)")
    L += ["", "## Source board (rows) to target board (columns): Should work / Could work / Does not exist / Does not make sense", ""]
    heads = list(boards)
    L.append("| Source | n | " + " | ".join(boards[t]["name"].replace("LP-", "").replace("LAUNCHXL-", "") for t in heads) + " |")
    L.append("|---|---|" + "|".join("---" for _ in heads) + "|")
    nsrc = collections.Counter(ex["board"] for ex in inv)
    for s in srcs:
        row = [s, str(nsrc[s])]
        for t in heads:
            d = S["matrix"].get(f"{s}->{t}", {})
            if not d:
                row.append("self" if boards[t]["sdk_board"] == s else "")
            else:
                row.append(f"{d.get('Should work',0)} / {d.get('Could work',0)} / {d.get('Does not exist',0)} / {d.get('Does not make sense',0)}")
        L.append("| " + " | ".join(row) + " |")
    L += ["", "## The gaps, ranked (what stops the most cells)", "", "Blocker text is the rule that fired; one line per (target, blocker), top 40.", "", "| Target | Blocker | Cells |", "|---|---|---|"]
    for b in S["blockers"][:40]:
        L.append(f"| {boards[b['target']]['name']} | {b['blocker']} | {b['n']:,} |")
    L += ["", "## Build queue: Should work cells by route, untried first", "", "Each line is one cross-build batch: every example of the source board whose verdict is Should work on the target through that layer. `evidence` says whether the bridge it rests on has been measured (a matrix.csv row), inferred from a neighbour, or never tried.", "", "| Source | Target | Layer | Bridge evidence | Examples |", "|---|---|---|---|---|"]
    for q in sorted(S["queue"], key=lambda q: ({"untried": 0, "inferred": 1, "measured": 2}[q["evidence"]], -q["n"])):
        L.append(f"| {q['source']} | {boards[q['target']]['name']} | {q['layer']} | {q['evidence']} | {q['n']} |")
    L += ["", "## What each target has (from its own SDK examples plus the hand entries in profiles.json)", ""]
    for t, c in S["caps"].items():
        L.append(f"- **{boards[t]['name']}**: {', '.join(c)}")
    open(os.path.join(HERE, "SUMMARY.md"), "w").write("\n".join(L) + "\n")

if __name__ == "__main__":
    main()
