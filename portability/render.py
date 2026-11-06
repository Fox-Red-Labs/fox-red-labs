#!/usr/bin/env python3
"""Render portability/matrix.csv to site/portability.html (local page, not published).
"Portability, proven" (2026-10-26). Order: the question and the sum of what ran (counts from matrix.csv), the works-with grid
(data/support-matrix.json, tools/works_with.py), the family-to-family map from matrix.csv, one card per test (route, layer,
verdict, how far up the ladder it got, counts, and what is in the way), the portability milestones (data/milestones.json, via
tools/render_milestones.py) and the FAQ. No roadmap and no untried cells: a map cell with no row is blank. The suggestion
column in matrix.csv is not rendered. Run after every change to matrix.csv. Never hand-edit the HTML."""
import csv, html, pathlib, datetime, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import site_out as so
import site_chrome as sc_nav
import render_boards as rb
from public_text import scrub
rows = list(csv.DictReader(open(ROOT / "portability/matrix.csv")))
LADDER = ["inspected", "built", "ran", "behavior verified"]
LAYERS = ["Plain C", "Zephyr app", "TI Drivers", "DriverLib", "Vendor HAL", "Radio stacks"]

def verdict_class(v):
    v = v.lower()
    for key, cls in [("does not make sense", "na"), ("does not exist", "none"), ("to test", "todo"),
                     ("should work", "should"), ("could work", "could"), ("works", "works")]:
        if v.startswith(key):
            return cls
    return "todo"

def counts(r):
    n = r["sample"]
    if not n:
        return ""
    parts = []
    for key, label in [("unchanged", "unchanged"), ("with_glue", "with glue"), ("after_edits", "after edits")]:
        if r[key]:
            parts.append(f"<b>{r[key]}</b> {label}")
    return f"{' · '.join(parts)} <span class=of>of {n}</span>" if parts else f"{n} tested"

def ladder(level):
    if level == "n/a":
        return ""
    reached = LADDER.index(level) if level in LADDER else -1
    steps = "".join(f'<li class="{"on" if i <= reached else ""}">{s}</li>' for i, s in enumerate(LADDER))
    return f'<ol class=ladder aria-label="Evidence: {html.escape(level)}">{steps}</ol>'

sys.path.insert(0, str(ROOT / "tools"))
from reader_check import ships  # noqa: E402


def source_links(src):
    """Each repo path in a source note becomes a link (the site sits one folder down); a bare file name after
    "and" takes the folder of the path before it. Words that are not files stay text."""
    e = html.escape
    out, folder = [], ""
    for tok in re.split(r"(\s+|;|,)", src):
        path = tok.strip().rstrip(".)")
        cand = path if "/" in path else (folder + path if folder and "." in path else "")
        if cand and (ROOT / cand).exists():
            folder = cand.rsplit("/", 1)[0] + "/"
            if ships(cand):
                out.append(f'<a href="../{e(cand)}">{e(tok)}</a>')   # a file the reader copy holds back gets no pointer
        else:
            out.append(e(tok))
    txt = "".join(out)
    txt = re.sub(r"(?:\s*[;,]\s*){2,}", "; ", txt)           # separators left bare by a dropped file
    txt = re.sub(r"^(?:\s*[;,]\s*)+|(?:\s*[;,]\s*)+$", "", txt).strip()
    return re.sub(r"^(?:and|or)\s+", "", txt)


def card(r):
    e = html.escape
    return f"""<article class="card v-{verdict_class(r['verdict'])}" id="{e(r['id'].lower())}">
  <header><span class=route>{e(r['from'])} <i>to</i> {e(r['to'])}</span><span class=pill>{e(r['verdict'])}</span></header>
  <p class=test>{e(r['test'])}</p>
  <p class=counts>{counts(r)}</p>
  {ladder(r['evidence_level'])}
  <p class=blk><span>What is in the way</span>{e(scrub(r['blocker']))}</p>
  <footer class=mono>{' · '.join(x for x in (e(r['id']), source_links(r['source']), e(r['date'])) if x)}</footer>
</article>"""

import json
import render_milestones as rm
import render_ports as rpt
import render_lab_coverage as rlc
SUPPORT = rpt.public_support(json.loads((ROOT / "data/support-matrix.json").read_text()))   # rows of private ports left off
e = html.escape

# family of a part name in matrix.csv; None leaves the row out of the map (other vendors, mixed sources)
FAM = [("MSP430", "MSP430"), ("MSPM33", "MSPM33"), ("MSPM0 or", None), ("MSPM0", "MSPM0"), ("CC35", "CC35xx"), ("CC23", "CC23xx, CC27xx, CC13xx"),
       ("CC27", "CC23xx, CC27xx, CC13xx"), ("CC13", "CC23xx, CC27xx, CC13xx"), ("AM13", "AM13"), ("AM263", "AM26x"),
       ("IWRL", "mmWave radar"), ("TDA4", "TDA4 Linux"), ("F28", "C2000")]
FAM_ORDER = ["MSP430", "MSPM0", "MSPM33", "CC23xx, CC27xx, CC13xx", "CC35xx", "AM13", "AM26x", "mmWave radar", "TDA4 Linux", "C2000"]
RANK = {"works": 5, "should": 4, "could": 3, "todo": 2, "none": 1, "na": 0}
RAN = {"ran", "behavior verified"}


def fam(part):
    for pre, f in FAM:
        if part.startswith(pre):
            return f
    return None


# what each run needed to move, read from the record's notes; the map shows the best layer that ran
NEEDS = {"X1": "header", "X13": "header", "X5": "board file", "X8": "config", "X31": "config + header",
         "X28": "as is", "X29": "config", "X32": "build files", "X33": "as is", "X48": "small edits",
         "X37": "rewritten", "X39": "board file", "X40": "board file", "X41": "board file", "X34": "as is",
         "X35": "header", "X49": "as is", "X50": "as is", "X2": "header", "X26": "as is", "X27": "as is",
         "X36": "header", "X38": "small edits"}
LAYER_RANK = {"TI Drivers": 3, "DriverLib": 3, "MCU+ drivers": 3, "Radio stacks": 3, "Zephyr app": 2, "Plain C": 1}


def family_map():
    cells, used_words = {}, set()
    for r in rows:
        a, b = fam(r["from"]), fam(r["to"])
        if a and b:
            cells.setdefault((a, b), []).append(r)
    used = [f for f in FAM_ORDER if any(f in k for k in cells)]
    head = "".join(f"<th scope=col>{e(f)}</th>" for f in used)
    body = []
    for a in used:
        tds = []
        for b in used:
            rs = cells.get((a, b), [])
            if a == b:
                tds.append('<td class="fm-same"><b>&#10003;</b><span>same SDK</span></td>')
                continue
            if not rs:
                tds.append('<td class="fm-empty"></td>')
                continue
            ran = sorted([r for r in rs if r["evidence_level"] in RAN], key=lambda r: -LAYER_RANK.get(r["layer"], 0))
            if ran:
                best, word, cls = ran[0], "Works", "fm-works"
            else:   # rows but none ran: the best matrix verdict word for the pair; its card names the blocker
                best = max(rs, key=lambda r: (RANK[verdict_class(r["verdict"])], LADDER.index(r["evidence_level"]) if r["evidence_level"] in LADDER else 0))
                vc = verdict_class(best["verdict"])
                word, cls = best["verdict"].split(" (")[0], "fm-" + vc
            used_words.add(word)
            need = NEEDS.get(best["id"], "") if ran else ""
            line = e(best["layer"]) + (": " + e(need) if need else "")
            tds.append(f'<td class="{cls}"><a href="#{e(best["id"].lower())}"><b>{e(word)}</b><span>{line}</span></a></td>')
        body.append(f"<tr><th scope=row>{e(a)}</th>{''.join(tds)}</tr>")
    key = ('<p class="small fm-key"><b>Works</b>: TI examples moved to that family ran on its board. Under it, the layer that ran and what the move needs: '
           '<b>as is</b> (rebuilt unchanged), <b>config</b> (new device and pins in SysConfig), <b>header</b> (one mapping header from this repository, app unchanged), '
           '<b>board file</b> (timer, pins and console for the new board), <b>small edits</b> (a few app lines).'
           + (' <b>Should work</b>: it built for the target; the same API is on both sides and the card names the glue in the way.' if "Should work" in used_words else "")
           + (' <b>Could work</b>: it needs real porting effort; the card names the blocker.' if "Could work" in used_words else "")
           + (' <b>Does not exist</b>: there is no layer to port to.' if "Does not exist" in used_words else "")
           + '</p>')
    return (f'<div class="fm-wrap"><table class="fm"><thead><tr><th scope=col class="fm-corner">From \\ to</th>{head}</tr></thead>'
            f'<tbody>{"".join(body)}</tbody></table></div>{key}')


import works_with as ww
STATE = ww.STATE


def works_with():
    return ww.grid(SUPPORT)


def faq():
    from site_text import inline
    return '<div class="pt-faq">' + "".join(f'<details><summary>{e(f["q"])}</summary><p>{inline(f["a"])}</p></details>' for f in SUPPORT["faq"]) + "</div>"


sections = []
for layer in LAYERS + sorted({r["layer"] for r in rows} - set(LAYERS)):
    rs = [r for r in rows if r["layer"] == layer]
    if rs:
        mix = {}
        for r in rs:
            mix[r["verdict"].split(" (")[0]] = mix.get(r["verdict"].split(" (")[0], 0) + 1
        mix_s = ", ".join(f"{n} {v.lower()}" for v, n in sorted(mix.items(), key=lambda kv: -kv[1]))
        sections.append(f'<details class="layer"><summary><h3>{e(layer)}</h3><span>{len(rs)} test{"s" if len(rs) > 1 else ""}: {e(mix_s)}</span></summary><div class=grid>{"".join(card(r) for r in rs)}</div></details>')

tally = {}
for r in rows:
    tally[verdict_class(r["verdict"])] = tally.get(verdict_class(r["verdict"]), 0) + 1
legend = [("works", "Works", "built and ran on the target"), ("should", "Should work", "same API, only glue in the way"),
          ("could", "Could work", "real porting effort"), ("none", "Does not exist", "no layer to port to"),
          ("na", "Does not make sense", "hardware is missing")]
legend_html = "".join(f'<li class="v-{c}"><span class=pill>{t}</span> {d} <b>{tally.get(c, 0)}</b></li>' for c, t, d in legend if tally.get(c, 0))

ran_rows = [r for r in rows if r["evidence_level"] in RAN]
fam_pairs = {(fam(r["from"]), fam(r["to"])) for r in ran_rows if fam(r["from"]) and fam(r["to"]) and fam(r["from"]) != fam(r["to"])}
fams_touched = {f for r in rows for f in (fam(r["from"]), fam(r["to"])) if f}
ms_port = sorted([m for m in json.loads((ROOT / "data/milestones.json").read_text())["milestones"] if m["area"] == "portability"], key=lambda m: m["date"], reverse=True)
updated = max(r["date"] for r in rows)
stats = [(str(len(rows)), "tests, one card each", "#every-test"), (str(len(ran_rows)), "ran on the target board", "#every-test"),
         (str(len(fam_pairs)), "family-to-family routes run on silicon", "#map"), (str(len(fams_touched)), "TI families on the map", "#map")]
_ran, _all = len(ran_rows), len(rows)
stats_html = (f'<div class="sbar" role="img" aria-label="{_all} tests: {_ran} ran on the target board">'
              f'<div class="sbar-track"><span class="sbar-run" style="width:{100 * _ran / max(1, _all):.2f}%"></span></div>'
              f'<p class="sbar-key"><span><i class="sbar-run"></i><b>{_ran}</b> of <b>{_all}</b> tests ran on the target board</span><span><i style="background:#E9E2D8"></i><b>{_all - _ran}</b> built or inspected only</span></p>'
              f'<p class="sbar-line"><a href="#map">{len(fam_pairs)} family-to-family routes</a> run on silicon, across {len(fams_touched)} TI families. <a href="#every-test">Every test</a> has a card.</p></div>')

lc_css, lc_grid, lc_questions, lc_script = rlc.sections()
_works, _verified = tally.get("works", 0), sum(1 for r in rows if r["evidence_level"] == "behavior verified")
css = f"""<style>
{rm.CSS}
{ww.CSS}
{lc_css}
main h2 {{ font-size:24px; margin:48px 0 6px; font-weight:600; }}
h2 + .sub {{ margin:0 0 14px; color:var(--frl-stone); max-width:70ch; }}
.lede {{ color:var(--frl-stone); max-width:62ch; margin:0 0 20px; }}
.answer {{ font-size:19px; line-height:28px; max-width:68ch; margin:8px 0 18px; }}
.mono {{ font-family:var(--frl-mono); font-size:12px; }}
.sbar {{ margin: 18px 0 10px; }} .sbar-track {{ display: flex; height: 22px; border-radius: 6px; overflow: hidden; background: #E9E2D8; }} .sbar-track span {{ display: block; height: 100%; }} .sbar-run {{ background: var(--frl-red); }} .sbar-part {{ background: #161312; }} .sbar-key {{ display: flex; flex-wrap: wrap; gap: 4px 18px; margin: 8px 0 0; font-size: 15px; color: var(--frl-stone); }} .sbar-key b {{ color: var(--frl-ink); font-weight: 700; }} .sbar-key i {{ display: inline-block; width: 10px; height: 10px; border-radius: 2px; margin-right: 6px; vertical-align: 0; }} .sbar-line {{ margin: 6px 0 0; font-size: 15px; color: var(--frl-stone); }}
.pt-stat {{ display:flex; flex-direction:column; gap:2px; background:#fff; border:1px solid var(--frl-line); border-radius:12px; padding:14px 16px; color:var(--frl-ink); text-decoration:none; }}
.pt-stat b {{ font-size:34px; line-height:1.05; color:var(--frl-red); }} .pt-stat span {{ font-size:14px; line-height:20px; color:var(--frl-stone); }}
.pt-spot {{ margin:16px 0 0; }} .pt-spot a {{ display:flex; flex-direction:column; gap:2px; background:#fff; border:1px solid var(--frl-line); border-left:4px solid var(--frl-red); border-radius:12px; padding:14px 18px; color:var(--frl-ink); text-decoration:none; max-width:760px; }} .pt-spot a:hover {{ border-color:var(--frl-stone); }} .pt-spot b {{ font-size:18px; }} .pt-spot span {{ font-size:15px; color:var(--frl-stone); }}
.pt-toc {{ display:flex; flex-wrap:wrap; gap:6px 20px; margin:16px 0 0; font-size:15px; }}
.fm-wrap {{ overflow-x:auto; max-width:100%; border:1px solid var(--frl-line); border-radius:12px; background:#fff; }}
table.fm {{ border-collapse:collapse; width:100%; font-size:13px; line-height:17px; display:table; }}
.fm th, .fm td {{ border-bottom:1px solid var(--frl-line); border-right:1px solid var(--frl-line); padding:8px 10px; text-align:left; vertical-align:top; }}
.fm thead th {{ font-weight:600; font-size:12px; letter-spacing:.02em; color:var(--frl-stone); background:var(--frl-cream); }}
.fm tbody th {{ font-weight:600; white-space:nowrap; background:var(--frl-paper); }}
.fm td {{ min-width:96px; }} .fm td a {{ display:block; text-decoration:none; color:var(--c); }}
.fm td b {{ display:block; font-size:13px; }} .fm td span {{ color:var(--frl-stone); font-size:12px; }}
.fm td[class^="v-"]:not(.fm-empty) {{ box-shadow: inset 4px 0 0 var(--c); }}
.fm-same {{ background:#EEF5F0; color:#2F6B4F; }} .fm-works {{ background:#E3EFE7; --c:#2F6B4F; }} .fm-should {{ background:#FBF3E2; --c:#8A5A00; }} .fm-could {{ background:#FBF3E2; --c:#8A5A00; }} .fm-none, .fm-na {{ --c:#6B625B; }}
.fm-key {{ margin-top:10px; }}
.fm-corner {{ color:var(--frl-stone); }}
.pt-faq details {{ border-top:1px solid var(--frl-line); padding:4px 0; }} .pt-faq summary {{ cursor:pointer; font-weight:600; font-size:17px; padding:10px 0; }}
.pt-faq p {{ margin:0 0 12px; font-size:16px; line-height:24px; max-width:72ch; }}
details.layer {{ border-top:1px solid var(--frl-line); padding:4px 0; }}
details.layer summary {{ cursor:pointer; list-style:none; display:flex; flex-wrap:wrap; align-items:baseline; gap:4px 14px; padding:12px 0; }}
details.layer summary::-webkit-details-marker {{ display:none; }}
details.layer summary h3 {{ margin:0; font-size:19px; }} details.layer summary h3::before {{ content:"+ "; color:var(--frl-red); }}
details.layer[open] summary h3::before {{ content:"- "; }}
details.layer summary span {{ color:var(--frl-stone); font-size:14px; }}
.legend {{ list-style:none; padding:0; margin:0; display:flex; flex-wrap:wrap; gap:8px 18px; font-size:14px; color:var(--frl-stone); }}
.legend b {{ color:var(--frl-ink); margin-left:4px; }}
.grid {{ display:grid; grid-template-columns:repeat(auto-fill,minmax(min(320px,100%),1fr)); gap:16px; margin:8px 0 16px; }}
.card {{ background:#fff; border:1px solid var(--frl-line); border-left:5px solid var(--c); border-radius:8px; padding:14px 16px; display:flex; flex-direction:column; gap:8px; min-width:0; }}
.card header {{ display:flex; justify-content:space-between; align-items:flex-start; gap:10px; }}
.route {{ font-weight:600; font-size:18px; }} .route i {{ font-style:normal; color:var(--frl-stone); font-weight:400; }}
.pill {{ display:inline-block; font-size:12px; font-weight:600; padding:2px 8px; border-radius:4px; border:1.5px solid var(--c); color:var(--c); white-space:nowrap; }}
.card p {{ margin:0; font-size:15px; }}
.test {{ color:var(--frl-stone); }}
.counts b {{ font-size:22px; font-weight:700; }} .counts .of {{ color:var(--frl-stone); }}
.blk span {{ display:block; font-size:11px; letter-spacing:.06em; text-transform:uppercase; color:var(--frl-stone); font-weight:600; }}
.ladder {{ list-style:none; display:flex; gap:4px; padding:0; margin:0; font-size:11px; }}
.ladder li {{ flex:1; text-align:center; padding:3px 2px; border-radius:4px; background:var(--frl-cream); color:var(--frl-stone); }}
.ladder li.on {{ background:var(--c); color:#fff; }}
.card footer {{ color:var(--frl-stone); margin-top:auto; word-break:break-all; font-size:11px; }}
.card footer a {{ color:var(--frl-stone); }}
.v-works {{ --c:var(--frl-ok); }} .v-should {{ --c:var(--frl-red); }} .v-could {{ --c:var(--frl-amber); }}
.v-none {{ --c:var(--frl-ink); }} .v-na {{ --c:var(--frl-stone); }} .v-todo {{ --c:var(--frl-stone); }}
.v-todo.card {{ border-left-style:dashed; }}
.pt-def {{ font-size:18px; line-height:28px; max-width:760px; margin:12px 0 0; }}
</style>"""
body = f"""<p class="crumb"><a href="index.html">Fox Red Labs</a> / <a href="ports.html">Software</a> / Portability, proven</p>
{sc_nav.tabs("software", "portability-proven.html")}
<h1 class="q">Which code moves between TI chip families?</h1>
<p class="pt-def"><b>Portable here</b> means one application file that never changes, plus one small glue file per chip family (35 to 66 lines). Moving inside a family costs a pin setup file only. If you know Arduino: the app is your sketch and the glue is the board core.</p>
<p class="q-sub">Proven by running: {_all} tests of moving code between MSP430, MSPM0, MSPM33, the CC23xx and CC27xx radios, CC35xx Wi-Fi, AM13, AM26x, C2000, radar and Linux. {_works} Work, {_ran} of {_all} ran on silicon and {_verified} had their behavior checked.</p>
<article class="pt-spot"><a href="one-app-eleven-boards.html"><b>One C file, eleven TI boards</b><span>The same app.c ran unchanged from an MSPM0 to AM62L Linux. What a move costs: one glue file of 35 to 66 lines per family.</span></a></article>
<p class="answer">Inside a family, often. Of 361 MSPM0 examples moved to a smaller MSPM0, 153 built and all 133 that were safe to run ran. Every driver example the CC23xx and CC27xx boards both ship (33 of 33) builds for either from the same source. Across families, in part, with a short header. MSPM0 examples ran on the Bluetooth CC2340R5 (5 of 10), the new MSPM33 (40) and the AM13 motor chip (14). TI's own C ran unchanged up to a Linux processor. MSP430 examples were rewritten for an MSPM0 and ran, and the post-quantum code written for the CC1354P10 passes its known-answer tests on the MSPM33, two MSPM0 parts and the CC2755P10. A C2000 current loop runs on the AM13 and AM26x from the same file. Each card names what stops the rest.</p>
{stats_html}
<p class="small">Terms: <a href="../docs/vocab.md">vocab</a> (SysConfig, DriverLib, TI Drivers, glue, shim, HAL). Retargeted means the same example rebuilt for another chip, usually by changing the device in SysConfig. A SysConfig translation rewrites the setup file for a chip with different drivers. Tested on one Intel Mac, macOS 15.7, with CCStudio 21.0.1 (TI Arm Clang 5.1.1), SysConfig 1.26.3, MSPM0 SDK 2.11.00.07, F3 SDK 9.21.00.36 and Zephyr 4.4.2 unless a card says otherwise. Questions: <a href="#faq">FAQ</a>.</p>
<nav class="pt-toc" aria-label="On this page"><a href="#what-works">What works with what</a><a href="#map">The map</a><a href="#every-test">Every test</a><a href="#milestones">Milestones</a><a href="#faq">FAQ</a></nav>

{lc_grid}

<h2 id="map">The map</h2>
<p class="sub">Code from the row's family moved to the column's family. The map assumes the target chip has the hardware: an AES example does not move to a chip with no AES block, and is not counted against the move. Click a cell for its record below.</p>
{family_map()}

<h2 id="every-test">Every test</h2>
<p class="sub">Each card is one test: code from one chip built, and where possible run, on another. It shows how far up the ladder it got and what is in the way. Red means the API already matches and only naming, packaging or build glue is in the way. Ports of other ecosystems (ESPHome, Arduino, other vendors' examples) are on <a href="ports.html">Start with your code</a>. From <a class=mono href="../portability/matrix.csv">portability/matrix.csv</a>, updated {e(updated)}.</p>
<ul class=legend>{legend_html}</ul>
{''.join(sections)}

<h2 id="milestones">Milestones</h2>
<p class="sub">The routes that ran, each with its limits and the file that holds the run. <a href="milestones.html">Every milestone, including tools and ecosystems</a>.</p>
{rm.cards(ms_port, compact=True)}

<h2 id="faq">FAQ</h2>
{faq()}
{lc_questions}
<script>{lc_script}</script>
<script>function frlOpen() {{ if (!location.hash) return; const h = location.hash.slice(1); if (h === "next" || h === "next-tests" || h === "builds") {{ history.replaceState(null, "", location.pathname); scrollTo(0, 0); return; }} const t = document.getElementById(h); const d = t && t.closest("details"); if (d && !d.open) {{ d.open = true; t.scrollIntoView(); }} }} frlOpen(); addEventListener("hashchange", frlOpen);</script>
"""
page = rb.page("Portability, proven", "Which code moves between TI chip families, proven by running: what works with what, the family-to-family map, one card per test and the milestones.", css + body).replace('<a href="hardware.html" aria-current="page">', '<a href="hardware.html">')
out = so.out_dir() / "portability-proven.html"
if "--check" in sys.argv:   # site_build.py check: compare without seo_pass's block, never write
    import re as _re
    _strip = lambda t: _re.sub(r"\n?<!-- seo -->.*?<!-- /seo -->", "", t, flags=_re.S)
    _same = out.exists() and _strip(out.read_text()) == _strip(page)
    print("portability page OK" if _same else f"DRIFT: {out}; run python3 portability/render.py")
    sys.exit(0 if _same else 1)
out.write_text(page)
print(f"wrote {so.rel(out)}: {len(rows)} tests")
