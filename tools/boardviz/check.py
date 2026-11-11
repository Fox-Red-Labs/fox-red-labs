#!/usr/bin/env python3
"""Check every data/boardviz/<board>.json: known part types, parts inside the board, unique ids, a label with a title and text,
header pin names shaped cols x rows, and an id that is a board (a results file) or a part in data/parts.json (an add-on a scene
wires to, like a screen). Exit 1 with the reasons, else print OK."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TYPES = set(re.findall(r"^  (\w+)\(", (ROOT / "site" / "boardviz" / "boardviz.js").read_text(), re.M))
# hobby parts (site/boardviz/hobby-parts.js): the types it builds, and PINS, the pins of each as places a wire can end
_HOBBY_JS = (ROOT / "site" / "boardviz" / "hobby-parts.js").read_text()
HOBBY = set(re.findall(r"^  (\w+)\(p\) \{", _HOBBY_JS, re.M))
HOBBY_PINS = json.loads(re.search(r"const PINS = (\{.*\});\n", _HOBBY_JS).group(1))
HOLE = re.compile(r"^(?:[a-j](?:[1-9]|[12]\d|30)|(?:tp|tm|bp|bm)(?:[1-9]|1\d|2[0-5]))$")   # a breadboard place: a column hole "j12" or a rail hole "tm5"
TAG = re.compile(r"^J(\d+) pin (\d+) \((\w+)\)$")   # how a wire end names a BoosterPack header pin


def check():
    problems = []
    known = {f.stem for f in (ROOT / "data" / "results").glob("*.csv")}
    parts = json.loads((ROOT / "data" / "parts.json").read_text()); parts = parts["parts"] if isinstance(parts, dict) else parts
    known |= {p["id"] for p in parts}
    for f in sorted((ROOT / "data" / "boardviz").glob("*.json")):
        d = json.loads(f.read_text()); bid = f.stem
        if d.get("id") != bid:
            problems.append(f"{f.name}: id {d.get('id')!r} is not the file name")
        if bid not in known:
            problems.append(f"{f.name}: {bid} is neither a board (data/results/{bid}.csv) nor a part in data/parts.json")
        W, H = d["size"]; seen = set()
        for i, p in enumerate(d.get("parts", []) + d.get("decor", [])):
            t = p.get("type")
            if t not in TYPES:
                problems.append(f"{f.name}: part {i} type {t!r} is not in boardviz.js"); continue
            if not (-4 <= p["x"] <= W + 4 and -4 <= p["y"] <= H + 4):
                problems.append(f"{f.name}: {t} at {p['x']},{p['y']} is off the {W}x{H} board")
            if p.get("id"):
                if p["id"] in seen: problems.append(f"{f.name}: id {p['id']} twice")
                seen.add(p["id"])
                if not (p.get("label") and p.get("title") and p.get("text")):
                    problems.append(f"{f.name}: part {p['id']} needs label, title and text")
            if p.get("names") and t not in ("terminal", "hallblock"):  # those two take one flat name per way
                c, r = p.get("cols", 1), p.get("rows", 1)
                if len(p["names"]) != r or any(len(row) != c for row in p["names"]):
                    problems.append(f"{f.name}: {p.get('id', t)} names are not {c}x{r}")
    # scenes: every board named has a table, every place names a part id that exists, every step has a title and text
    tables = {f.stem: json.loads(f.read_text()) for f in (ROOT / "data" / "boardviz").glob("*.json")}
    for f in sorted((ROOT / "data" / "boardviz" / "scenes").glob("*.json")):
        sc = json.loads(f.read_text()); boards = [sc.get("base")] + [b["id"] for b in sc.get("boards", [])]
        things = {t["id"] for t in sc.get("things", [])}
        by_id = {}
        for t in sc.get("things", []):
            if t["id"] in by_id: problems.append(f"scenes/{f.name}: thing id {t['id']} twice")
            by_id[t["id"]] = t
            if t["type"] not in TYPES and t["type"] not in HOBBY: problems.append(f"scenes/{f.name}: thing {t['id']} type {t['type']!r} is in neither boardviz.js nor hobby-parts.js")
        holes_used = {}   # a breadboard hole holds one lead
        def use_hole(h, who):
            if h in holes_used: problems.append(f"scenes/{f.name}: breadboard hole {h} holds {holes_used[h]} and {who}")
            holes_used[h] = who
        for t in sc.get("things", []):
            if t["type"] in HOBBY_PINS:
                for pin, ref in (t.get("plug") or {}).items():
                    if pin not in HOBBY_PINS[t["type"]]: problems.append(f"scenes/{f.name}: {t['id']} has no pin {pin}")
                    own, _, hn = ref.partition(".")
                    if by_id.get(own, {}).get("type") != "breadboard" or not HOLE.match(hn): problems.append(f"scenes/{f.name}: {t['id']}.{pin} plug {ref} is not a breadboard hole")
                    else: use_hole(hn, f"{t['id']}.{pin}")
                if t.get("plug") and set(t["plug"]) != set(HOBBY_PINS[t["type"]]): problems.append(f"scenes/{f.name}: {t['id']} plug must name every pin: {sorted(HOBBY_PINS[t['type']])}")
        def place_ok(ref):
            bits = ref.split("."); own = bits[0]
            if own in things:
                t = by_id[own]
                if len(bits) == 1: return True   # the whole thing (a focus, a switch-on)
                if t["type"] == "breadboard": return len(bits) == 2 and bool(HOLE.match(bits[1]))
                if t["type"] in HOBBY_PINS: return len(bits) == 2 and bits[1] in HOBBY_PINS[t["type"]]
                return True
            if not (own in ids and len(bits) > 1 and bits[1] in ids[own]): return False
            if len(bits) > 2 and bits[2].startswith("pin"):   # a header pin must exist
                p = next(q for q in tables[own]["parts"] if q.get("id") == bits[1]); return bits[2][3:].isdigit() and int(bits[2][3:]) < p.get("cols", 1) * p.get("rows", 1)
            return True
        def pin_tag_problem(ref, tag):   # "J4 pin 38 (PA28)" must be what the header table says for that pin
            bits = ref.split(".")
            if len(bits) < 3 or not bits[2].startswith("pin") or bits[0] not in tables or not TAG.match(tag or "") or not place_ok(ref): return None
            tb, tp = bits[0], bits[1]
            mated = next((b for b in sc.get("boards", []) if b["id"] == tb and b.get("mate")), None)
            if mated and tp in ("t1", "t2"):   # a stack-through pin on a mated BoosterPack is the LaunchPad kit's own pin: the tag names the LaunchPad kit's header table
                tb, tp = sc["base"], {"t1": "j1", "t2": "j2"}[tp]
            p = next(q for q in tables[tb]["parts"] if q.get("id") == tp); k = int(bits[2][3:]); r = p.get("rows", 1); i, j = divmod(k, r)
            no, name = p["pinno"][j][i], p["names"][j][i]; j_no = (no - 1) // 10 + 1
            return None if tag == f"J{j_no} pin {no} ({name})" else f"{ref} is J{j_no} pin {no} ({name}) but the wire tag says {tag!r}"
        for b in boards:
            if b not in tables:
                problems.append(f"scenes/{f.name}: no data/boardviz/{b}.json")
        ids = {b: {q.get("id") for q in tables[b].get("parts", []) if q.get("id")} for b in boards if b in tables}
        for b in sc.get("boards", []):
            for mine, theirs in b.get("mate", []):
                if b["id"] in ids and mine not in ids[b["id"]]: problems.append(f"scenes/{f.name}: {b['id']} has no part {mine} to mate")
                if sc["base"] in ids and theirs not in ids[sc["base"]]: problems.append(f"scenes/{f.name}: {sc['base']} has no part {theirs} to mate")
        for w in sc.get("wires", []):
            for end in ("from", "to"):
                if not place_ok(w[end]): problems.append(f"scenes/{f.name}: wire {w['id']} {end} {w[end]} names no part")
            ends = w.get("ends") or []
            for end, k in (("from", 0), ("to", 1)):
                if w[end].startswith("bb."): use_hole(w[end][3:], f"wire {w['id']}")
                bad = pin_tag_problem(w[end], ends[k] if len(ends) == 2 else None)
                if bad: problems.append(f"scenes/{f.name}: wire {w['id']}: {bad}")
            if len(ends) != 2 or not ends[1]: problems.append(f"scenes/{f.name}: wire {w['id']} needs ends [far end or null, board end]: the tags and the wiring table read them")
            if not w.get("label"): problems.append(f"scenes/{f.name}: wire {w['id']} needs a label for the wiring table")
        c = sc.get("console")
        if c and not (c.get("href") and c.get("label") and (ROOT / "site" / c["href"]).exists()):
            problems.append(f"scenes/{f.name}: console needs href (a site page that exists) and label")
        pins = sc.get("pins")
        if pins:
            if not pins.get("source") or not (ROOT / pins["source"]).exists(): problems.append(f"scenes/{f.name}: pins.source must name a repo file")
            for r in pins.get("rows", []):
                if len(r) != 4: problems.append(f"scenes/{f.name}: pins row {r[:1]} needs 4 cells")
        for i, st in enumerate(sc.get("steps", [])):
            if not (st.get("title") and st.get("text")): problems.append(f"scenes/{f.name}: step {i + 1} needs title and text")
            if st.get("focus") and not place_ok(st["focus"]): problems.append(f"scenes/{f.name}: step {i + 1} focus {st['focus']} names no part")
            for a in st.get("do", []):
                for k in ("switch", "dip", "shunt", "spin", "stop", "drop", "lift", "show", "hide"):
                    if k in a and not (place_ok(a[k]) or a[k] in things or a[k] in ids): problems.append(f"scenes/{f.name}: step {i + 1} {k} {a[k]} names nothing")
                for ref in ([a["led"]] if isinstance(a.get("led"), str) else a.get("led", [])):
                    if not place_ok(ref): problems.append(f"scenes/{f.name}: step {i + 1} led {ref} names no part")
                for ref in ([a["flow"]] if isinstance(a.get("flow"), str) else a.get("flow", [])) + ([a["noflow"]] if isinstance(a.get("noflow"), str) else a.get("noflow", [])):
                    if ref not in {w["id"] for w in sc.get("wires", [])}: problems.append(f"scenes/{f.name}: step {i + 1} flow {ref} is not in wires")
                for ref in ([a["wire"]] if isinstance(a.get("wire"), str) else a.get("wire", [])):
                    if ref not in {w["id"] for w in sc.get("wires", [])}: problems.append(f"scenes/{f.name}: step {i + 1} wire {ref} is not in wires")
    # generated files: the breadboard scenes (hobby_scenes.py) and the printable diagrams made from the scenes
    # (wiring_still.py). A hand edit or a changed source leaves them stale. Each is checked where its generator is.
    import importlib.util, io, contextlib
    for name in ("hobby_scenes", "wiring_still"):
        gen = ROOT / "tools" / "boardviz" / f"{name}.py"
        if not gen.exists():
            continue
        spec = importlib.util.spec_from_file_location(name, gen); mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        with contextlib.redirect_stdout(io.StringIO()) as buf:
            stale = mod.main(["--check"])
        if stale: problems.append(buf.getvalue().strip())
    return problems


if __name__ == "__main__":
    ps = check()
    print("\n".join(ps) if ps else "OK")
    sys.exit(1 if ps else 0)
