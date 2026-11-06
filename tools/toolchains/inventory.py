#!/usr/bin/env python3
"""Count which toolchain project folders TI ships per example, per SDK and board.

Read only. Walks the installed TI SDKs, finds every example that ships a project
for a toolchain (TI Arm Clang, Arm GCC, IAR, Keil, CCS for C2000, CMake), and
writes data/toolchains/claims.csv.

Usage:
  python3 tools/toolchains/inventory.py            write claims.csv, print summary
  python3 tools/toolchains/inventory.py --summary  print summary only
  python3 tools/toolchains/inventory.py --markdown print the README table
  python3 tools/toolchains/inventory.py --check-quotes
                                                   confirm each quoted line in
                                                   sdk-compilers.json is still in its file

An "example" is one (board, kind, example folder). The same example on two boards
counts twice. kind is nortos, freertos, tirtos7, rtos (no RTOS subfolder), system
(multi-core system project), or for C2000 the area of the tree.
A toolchain counts for an example when its folder holds a project file:
  projectspec or makefile (ticlang, gcc), .ewp (iar), .uvprojx/.uvproj/.cprj (keil),
  .projectspec (ccs, C2000), CMakeLists.txt next to the example (cmake).
"""
import csv
import json
import os
import re
import sys
from collections import Counter, defaultdict

HOME = os.path.expanduser("~")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_CSV = os.path.join(REPO, "data", "toolchains", "claims.csv")
OUT_JSON = os.path.join(REPO, "data", "toolchains", "sdk-compilers.json")

# name, root, style, scan subfolders
SDKS = [
    ("mspm0_sdk_2_11_00_07", "/Applications/ti/mspm0_sdk_2_11_00_07", "os_board", ["examples"]),
    ("mspm33_sdk_1_04_00_00", "/Applications/ti/mspm33_sdk_1_04_00_00", "os_board", ["examples"]),
    ("am13e230x_sdk_26_01_00_03", "/Applications/ti/am13e230x_sdk_26_01_00_03", "core_os", ["examples", "solutions"]),
    ("simplelink_lowpower_f3_sdk_9_21_00_36_LTS", "/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS", "os_board", ["examples"]),
    ("simplelink_wifi_sdk_10_20_00_39", "/Applications/ti/simplelink_wifi_sdk_10_20_00_39", "os_board", ["examples"]),
    ("c2000ware_26_02_00_00", "/Applications/ti/c2000/C2000Ware_26_02_00_00", "c2000", ["."]),
    ("c2000ware_core_26_02_00_00", "/Applications/ti/c2000ware_core_26_02_00_00", "c2000", ["."]),
    ("mcu_plus_sdk_am263px_26_01_00_08", HOME + "/ti/mcu_plus_sdk_am263px_26_01_00_08", "core_os", ["examples"]),
    ("mmwave_l_sdk_05_05_04_02", HOME + "/ti/mmwave_l_sdk_05_05_04_02", "core_os", ["examples"]),
]

TC_DIRS = {
    "ticlang": "ticlang", "ti_arm_clang": "ticlang", "ti-arm-clang": "ticlang",
    "gcc": "gcc", "gcc_arm": "gcc",
    "iar": "iar", "iar_arm": "iar",
    "keil": "keil", "keil_arm_clang": "keil",
}
RTOS_DIRS = {"freertos", "tirtos7", "tirtos", "threadx", "zephyr"}
DEVICE_RE = re.compile(r"^(f2\d\w*|mcpc\d+)$", re.I)
TOOLCHAINS = ["ticlang", "gcc", "iar", "keil", "ccs", "cmake"]


def project_types(files):
    """Project file types present in one folder."""
    t = set()
    for f in files:
        low = f.lower()
        if low.endswith(".projectspec"):
            t.add("projectspec")
        elif low in ("makefile",):
            t.add("makefile")
        elif low.endswith(".ewp"):
            t.add("ewp")
        elif low.endswith((".uvprojx", ".uvproj")):
            t.add("uvprojx")
        elif low.endswith(".cprj"):
            t.add("cprj")
    return t


def read_head(path, n=20000):
    try:
        with open(path, "r", errors="replace") as fh:
            return fh.read(n)
    except OSError:
        return ""


def scan_standard(name, root, style, subs, ex, stats, observed):
    """ex: dict (board, kind, example_id) -> {toolchain: set(file_types)}"""
    for sub in subs:
        base = os.path.join(root, sub)
        if not os.path.isdir(base):
            continue
        for dp, dns, fns in os.walk(base):
            rel = os.path.relpath(dp, base)
            if rel == ".":
                continue
            parts = rel.split(os.sep)
            bn = parts[-1]
            types = project_types(fns)
            has_cmake = "CMakeLists.txt" in fns
            tc = TC_DIRS.get(bn)
            if style == "core_os" and not tc and bn.startswith("system") and "projectspec" in types:
                # multi-core system project: projectspec sits in the system_* folder
                if len(parts) >= 3:
                    board = parts[-2]
                    exid = "/".join([sub] + parts[:-2])
                    ex[(board, "system", exid)].setdefault("ticlang", set()).update(types)
                    note_attrs(dp, fns, "ticlang", observed)
                continue
            if style == "core_os" and has_cmake and not tc and len(parts) >= 3 and "_" in bn:
                board = parts[-2]
                kind = bn.split("_", 1)[1]
                exid = "/".join([sub] + parts[:-2])
                ex[(board, kind, exid)].setdefault("cmake", set()).add("CMakeLists.txt")
            if not tc:
                continue
            if not types:
                stats["tc_dirs_without_project"] += 1
                continue
            if style == "core_os":
                if len(parts) < 4:
                    continue
                core_os = parts[-2]
                board = parts[-3]
                kind = core_os.split("_", 1)[1] if "_" in core_os else core_os
                exid = "/".join([sub] + parts[:-3])
            else:  # os_board: <nortos|rtos>/<board>/.../<example>[/<rtos>]/<tc>
                if len(parts) < 4:
                    continue
                osdir, board = parts[0], parts[1]
                up = parts[:-1]
                kind = osdir
                if up[-1] in RTOS_DIRS:
                    kind = up[-1]
                    up = up[:-1]
                exid = "/".join(up[2:]) or "(board root)"
                exid = sub + "/" + osdir + "/" + exid
                # CMake file next to the example (USB examples)
                edir = os.path.join(base, *up)
                if os.path.isfile(os.path.join(edir, "CMakeLists.txt")):
                    ex[(board, kind, exid)].setdefault("cmake", set()).add("CMakeLists.txt")
            ex[(board, kind, exid)].setdefault(tc, set()).update(types)
            note_attrs(dp, fns, tc, observed)


def note_attrs(dp, fns, tc, observed):
    """Record compiler hints written inside project files (counts only)."""
    for f in fns:
        low = f.lower()
        if low.endswith(".projectspec"):
            h = read_head(os.path.join(dp, f))
            for key in ("toolChain", "cgtVersion", "compiler"):
                m = re.search(r'\b%s="([^"]*)"' % key, h)
                if m:
                    observed["projectspec_%s (%s dir)" % (key, tc)][m.group(1)] += 1
        elif low.endswith(".uvprojx"):
            h = read_head(os.path.join(dp, f), 6000)
            m = re.search(r"<pCCUsed>([^<]*)</pCCUsed>", h)
            if m:
                observed["keil_pCCUsed"][m.group(1)] += 1


def scan_c2000(name, root, stats, ex, observed):
    for dp, dns, fns in os.walk(root):
        for f in fns:
            if not f.lower().endswith(".projectspec"):
                continue
            path = os.path.join(dp, f)
            head = read_head(path, 4000)
            m = re.search(r'outputType="([^"]*)"', head)
            if m and m.group(1).lower() == "staticlibrary":
                stats["library_projectspecs_skipped"] += 1
                continue
            rel = os.path.relpath(path, root)
            parts = rel.split(os.sep)
            area = parts[0]
            if area == "driverlib" and "examples" not in parts:
                stats["library_projectspecs_skipped"] += 1
                continue
            board = "(none)"
            for p in parts[1:]:
                tok = p.split("_")[0].split(".")[0].lower()
                if DEVICE_RE.match(tok):
                    board = tok
                    break
            exid = rel[: -len(".projectspec")]
            ex[(board, area, exid)].setdefault("ccs", set()).add("projectspec")
            mv = re.search(r'cgtVersion="([^"]*)"', head)
            if mv:
                observed["projectspec_cgtVersion (ccs)"][mv.group(1)] += 1


def scan_sdk(name, root, style, subs):
    ex = defaultdict(dict)
    stats = Counter()
    observed = defaultdict(Counter)
    if not os.path.isdir(root):
        return None
    if style == "c2000":
        scan_c2000(name, root, stats, ex, observed)
    else:
        scan_standard(name, root, style, subs, ex, stats, observed)
    return ex, stats, observed


def build_rows(name, ex):
    board_total = Counter()
    kind_total = Counter()
    for (board, kind, _), tcs in ex.items():
        board_total[board] += 1
        kind_total[(board, kind)] += 1
    rows = []
    agg = defaultdict(lambda: [0, set()])
    for (board, kind, _), tcs in ex.items():
        for tc, types in tcs.items():
            a = agg[(board, kind, tc)]
            a[0] += 1
            a[1].update(types)
    for (board, kind, tc), (n, types) in sorted(agg.items()):
        rows.append({
            "sdk": name, "board": board, "rtos_or_kind": kind, "toolchain": tc,
            "examples": n, "file_types": "+".join(sorted(types)),
            "kind_total_examples": kind_total[(board, kind)],
            "board_total_examples": board_total[board],
        })
    return rows


def summarize(name, ex):
    tot = len(ex)
    c = Counter()
    for tcs in ex.values():
        for tc in tcs:
            c[tc] += 1
    return tot, c


def md_table(summ):
    head = "| SDK | examples | ticlang | gcc | iar | keil | ccs | cmake |\n|---|---|---|---|---|---|---|---|\n"
    lines = []
    for name, (tot, c) in summ:
        lines.append("| %s | %d | %s |" % (name, tot, " | ".join(str(c.get(t, 0)) for t in TOOLCHAINS)))
    return head + "\n".join(lines)


def norm(s):
    """Whitespace and tag-spacing tolerant form used to compare quotes."""
    s = re.sub(r"<[^>]*>", " ", s)
    s = re.sub(r"\s+", " ", s)
    return re.sub(r" ([.,;:])", r"\1", s).strip()


def check_quotes():
    if not os.path.isfile(OUT_JSON):
        print("no", OUT_JSON)
        return 1
    data = json.load(open(OUT_JSON))
    bad = 0
    for sdk in data["sdks"]:
        for c in sdk.get("compilers", []):
            q, f = c.get("quote"), c.get("file")
            if not q or not f:
                continue
            path = os.path.expanduser(f)
            try:
                text = open(path, errors="replace").read()
            except OSError:
                print("MISSING FILE", f)
                bad += 1
                continue
            if c.get("quote_checkable") is False:
                continue
            if norm(q) not in norm(text):
                print("QUOTE NOT FOUND in", f, ":", q)
                bad += 1
    print("quotes OK" if not bad else "%d quote problems" % bad)
    return 1 if bad else 0


def main():
    args = sys.argv[1:]
    if "--check-quotes" in args:
        sys.exit(check_quotes())
    all_rows, summ, extra = [], [], {}
    for name, root, style, subs in SDKS:
        res = scan_sdk(name, root, style, subs)
        if res is None:
            print("skip (not installed):", root, file=sys.stderr)
            continue
        ex, stats, observed = res
        all_rows += build_rows(name, ex)
        summ.append((name, summarize(name, ex)))
        extra[name] = (stats, observed)
    if "--markdown" in args:
        print(md_table(summ))
        return
    if "--summary" not in args:
        os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
        with open(OUT_CSV, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=["sdk", "board", "rtos_or_kind", "toolchain", "examples",
                                               "file_types", "kind_total_examples", "board_total_examples"])
            w.writeheader()
            w.writerows(all_rows)
        print("wrote", OUT_CSV, len(all_rows), "rows")
    print(md_table(summ))
    for name, (stats, observed) in extra.items():
        print("\n%s notes: %s" % (name, dict(stats)))
        for k, c in observed.items():
            print("  %s: %s" % (k, dict(c.most_common(8))))


if __name__ == "__main__":
    main()
