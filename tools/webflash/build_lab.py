#!/usr/bin/env python3
"""The Digital Lab's examples, chip view and debugger panel: the data and the generated markup block (concept 7, approved 2026-11-09).

  python3 tools/webflash/build_lab.py          writes site/flash/bench/lab-chip.json and the <!--lab-v7:examples--> block of index.html
  python3 tools/webflash/build_lab.py --check  exit 1 when either is stale (what site_check runs)

lab-chip.json has one entry per board that has data/chip-jobs/<board>.json, so the Lab offers the chip view for exactly those
boards and for any board added later with no code change: the chip data (what site/chip/chipview.js draws), the example cards
(title, one line, status, picture) and the pin table of each build. A board with no file gets no chip view. The card pictures
are the ones tools/board_chip.py already writes to site/media/cards/. The chip drawing itself is site/chip/chipview.js, imported
by lab.js and not copied. The Lab's engine (bench.js and friends) is not touched by this script."""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import board_chip as bc  # noqa: E402

BENCH = ROOT / "site" / "flash" / "bench"
PAGE = BENCH / "index.html"
OUT = BENCH / "lab-chip.json"
PHOTOS = {"mspm0g3507": "g3507-on-bench.jpg", "cc2340r5": "cc2340r5-blinking.jpg", "am13e230": "am13e230-on-bench.jpg",
          "mspm0g3519": "g3519-board-drawing.jpg", "mspm0g5187": "g5187-sensors-boosterpack-bench.jpg", "tda4": "sk-tda4vm-on-bench.jpg"}
M0, M1 = "<!--lab-v7:examples-->", "<!--/lab-v7:examples-->"

BLOCK = """<section id="lab-examples" aria-label="Examples" hidden>
  <div class="sec-h"><h2>Examples</h2><p class="small" id="lab-ex-sub"></p></div>
  <div id="lab-chip-mount"></div>
</section>
<template id="lab-chip-tpl"><div class="cv-page" id="cv-root">
  <div class="cv-cards" id="builds"></div>
  <section id="inside" aria-label="Inside the chip"><div class="cv-panel">
    <div class="cv-panel-h"><h2 id="cv-ptitle"></h2><p>How it runs inside the chip, drawn from the build's code. Click a step or a block to follow one job.</p></div>
    <p class="cv-wiring" id="cv-wiring"></p>
    <div class="cv-grid"><div class="cv-chipwrap"><div class="cv-mode" role="group" aria-label="Chip view"><button type="button" data-mode="focus" aria-pressed="false">Just what runs</button><button type="button" data-mode="whole" aria-pressed="false">Whole chip</button></div><div class="cv-frame"><div id="cv-chipmount"></div></div>
      <p class="cv-now" id="cv-now" aria-live="polite">Tap a block or a step to follow one job.</p><div class="cv-legend" id="cv-legend"></div></div>
      <div class="cv-listcol"><h3>The steps</h3><div id="cv-list"></div></div></div>
  </div></section>
</div></template>
"""


def status_of(t):
    """The Lab's status words are Works / Should work / Could work / Not for this chip; the board page's longer status keeps its detail after a colon."""
    if t.startswith("Works") or t.startswith("Ran on this board"):
        return "Works"
    if t.startswith("Not for"):
        return "Not for this chip"
    if "ran on" in t:
        return "Should work: ran on " + t.split("ran on", 1)[1].strip()
    return "Could work: not run on a board yet"


def page_unproven():
    """Boards whose browser write and put-back no run record proves (boards.json recovery.proven is false): the engine's banner says
    "hardware check pending" for them, so a card may not say Works, which would claim the page ran it."""
    try:
        bs = json.loads((BENCH / "boards.json").read_text())["boards"]
    except (OSError, ValueError, KeyError):
        return set()
    return {b["id"] for b in bs if (b.get("recovery") or {}).get("proven") is False}


def data():
    boards = {}
    for f in sorted(bc.JOBS.glob("*.json")):
        bid = f.stem
        d = bc.data(bid)
        cards = []
        for slug, b in d["builds"].items():
            ran_here = b["status"].startswith("Works") or b["status"].startswith("Ran on this board")
            works = ran_here   # a run on the board is a run (maintainer, 2026-11-09); the engine's own banner covers what this page has written
            status = status_of(b["status"])
            cards.append({"slug": slug, "title": b["card_title"], "line": b["card_line"], "status": status, "ran": works,
                          "img": "../../" + bc.card_src(bid, slug)})
        pins = {slug: [{"pin": p, "where": v["header"], "block": v["block"]} for p, v in b["pins"].items()] for slug, b in d["builds"].items()}
        boards[bid] = {"jobs": {**{k: d[k] for k in ("chip", "kinds", "blocks")}, "builds": {s_: {k: v for k, v in b_.items() if k != "status"} for s_, b_ in d["builds"].items()}}, "cards": cards, "pins": pins,
                       "legend": bc.legend_html(d), "map": f"../chip/maps/{bid}.json" if (ROOT / "site/flash/chip/maps" / f"{bid}.json").exists() else None}
    photos = {k: "../../media/" + v for k, v in PHOTOS.items() if (ROOT / "site/media" / v).exists()}
    for t in sorted((ROOT / "site/media/boards").glob("*.jpg")):   # the drawn board (tools/board_thumbs.py) before the bench photo
        photos[t.stem] = "../../media/boards/" + t.name
    return {"boards": boards, "photos": photos}


def render():
    j = json.dumps(data(), separators=(",", ":"), ensure_ascii=False) + "\n"
    page = PAGE.read_text()
    a, b = page.index(M0), page.index(M1)
    return j, page[: a + len(M0)] + "\n" + BLOCK + page[b:]


def main():
    j, page = render()
    stale = [n for n, cur, new in (("lab-chip.json", OUT.read_text() if OUT.exists() else "", j), ("index.html", PAGE.read_text(), page)) if cur != new]
    if "--check" in sys.argv:
        if stale:
            print("stale: " + ", ".join(stale) + " (run python3 tools/webflash/build_lab.py)")
            return 1
        print("OK: lab-chip.json and the Lab's examples block are current")
        return 0
    OUT.write_text(j)
    PAGE.write_text(page)
    print("wrote lab-chip.json and the lab-v7 block" if stale else "already current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
