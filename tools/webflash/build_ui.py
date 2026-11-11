#!/usr/bin/env python3
"""Keep the Digital Lab page's shared chrome and its take-it-home kit in step with the rest of the site.

  python3 tools/webflash/build_ui.py          rewrite the marked blocks of site/flash/bench/index.html and write kit.json
  python3 tools/webflash/build_ui.py --check  exit 1 when either is stale (what site_check runs)

site/flash/bench/index.html is hand-written. Four blocks inside it are generated and must not be edited by hand:
  <!--bench-chrome:header-->   the site header from tools/site_chrome.py, links rewritten for the page's depth (../../)
  <!--bench-chrome:footer-->   the site footer, same
  /*bench-chrome:css*/         the chrome CSS, the brand tokens and the kit CSS (tools/site_kit.KIT_CSS)
  <!--bench-chrome:kit-->      the "Take it home" markup, from tools/site_kit.kit(): the same Mac / Windows / Linux switch,
                               tick boxes and live preview as the board pages
site/flash/bench/demos.json holds the project demos the picker lists beside the bench's own; kit.json holds each bench board's checklist (tools/board_template.kit_options: the board's folder and
AGENTS.md, the vendor IDE and SDK, GCC and OpenOCD where recorded, Zephyr). ui.js rebuilds the tick boxes from it when the
connected board or the demo changes; the class names (k-*) and the copy logic match site_kit.KIT_JS, so a change to the board
pages' kit reaches this page by rerunning this script. Nothing here touches flashing, Web Serial or WebUSB."""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import site_chrome as sc   # noqa: E402
import site_kit as sk      # noqa: E402

BENCH = ROOT / "site" / "flash" / "bench"
PAGE = BENCH / "index.html"
FAMILY_FALLBACK = {"am13e230": "AM13", "mspm33c321a": "MSPM33", "cc35x1": "CC35xx"}
DEMO_TEXT = ("Then build and run the \"{title}\" demo for my {kit}, the one I just ran in the browser: follow {folder}/AGENTS.md "
             "and the matching guide in docs/builds, flash it and check the result.")
DEMO_TEXT_NOFOLDER = ("Then build and run the \"{title}\" demo for my {kit}, the one I just ran in the browser, flash it and check the result.")


def rebase(h):
    """Links in the site's header and footer are relative to site/; this page is two folders down."""
    h = re.sub(r'href="\.\./([^"]*)"', lambda m: f'href="../../../{m.group(1)}"', h)
    h = re.sub(r'href="(?!https?:|#|mailto:|\.\./|claude-cli:|javascript:)([^"]*)"', lambda m: f'href="../../{m.group(1)}"', h)
    return h.replace('action="search.html"', 'action="../../search.html"').replace('location.href = "search.html"', 'location.href = "../../search.html"')


# project-grid cell keys (site/data/projects.json) -> bench board ids; a project is listed for a board only where that board ran it
CELL_TO_BENCH = {"g3507": "mspm0g3507", "l1306": "mspm0l1306", "c1104": "mspm0c1104", "g3519": "mspm0g3519", "g5187": "mspm0g5187",
                 "am13e230": "am13e230", "mspm33c321a": "mspm33c321a", "cc2340r5": "cc2340r5", "cc2755p10": "cc2755p10", "cc35x1": "cc35x1"}
MAC_BOOT = "git sparse-checkout add mac-setup && ./mac-setup/bootstrap.sh"
OPENOCD_NOTE = "# xPack OpenOCD 0.12.0-7 or newer (the first with ti_mspm0.cfg): https://github.com/xpack-dev-tools/openocd-xpack/releases"


def demos_json():
    """The demo picker's project rows: the projects that ran end to end (they carry a recipe), with the status per bench board."""
    pj = json.loads((ROOT / "site/data/projects.json").read_text())["projects"]
    out = []
    for p in pj:
        if not p.get("recipe"):
            continue
        href = f"project-{p['slug']}.html"
        if not (ROOT / "site" / href).exists():
            continue
        cells = {}
        for k, v in {**p.get("cells", {}), **p.get("also", {})}.items():
            if k in CELL_TO_BENCH and isinstance(v, dict) and v.get("st"):
                cells[CELL_TO_BENCH[k]] = v["st"]
        out.append({"slug": p["slug"], "t": p["t"], "tier": p.get("tier", "medium"), "what": p.get("what", ""), "time": p.get("time", ""),
                    "href": "../../" + href, "boards": cells})
    return {"made_from": ["site/data/projects.json"], "projects": out}


def terminal_tools(bid, fam, routes_note=""):
    """The tool lines of the terminal copy, per OS: ide (CCStudio and the SDK) and free (GCC and OpenOCD, where a flash without an IDE is recorded)."""
    import board_template as bt
    ide = {"mac": [], "win": [], "linux": []}
    if fam in bt.SDKS:
        sdk, ver, slug, flag = bt.SDKS[fam]
        if flag is not None:
            ide["mac"] = [MAC_BOOT + flag]
        else:
            ide["mac"] = [MAC_BOOT, f"# then download the {sdk} {ver} from ti.com/tool/download/{slug}"]
        for o, name in (("win", "Windows"), ("linux", "Linux")):
            ide[o] = [f"# Download CCStudio {bt.CCS_VERSION} and the {sdk} {ver} for {name} from ti.com/tool/download/CCSTUDIO and ti.com/tool/download/{slug}, then run each installer."]
    elif fam in bt.CCS_ONLY:
        ide["mac"] = [MAC_BOOT]
        for o, name in (("win", "Windows"), ("linux", "Linux")):
            ide[o] = [f"# Download CCStudio {bt.CCS_VERSION} for {name} from ti.com/tool/download/CCSTUDIO, then run the installer."]
    free = {"mac": [], "win": [], "linux": []}
    if bid in bt.OPENOCD_BOARDS:
        if bid in bt.GCC_BOARDS:
            free = {"mac": ["brew install --cask gcc-arm-embedded", "# No admin password on this Mac? Unpack the Arm GNU Toolchain (darwin-arm64 or darwin-x86_64 .tar.xz) from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads into ~/ti and put its bin folder on PATH"], "win": ["winget install Arm.GnuArmEmbeddedToolchain"], "linux": ["sudo apt install gcc-arm-none-eabi"]}
        for o in free:
            free[o].append(OPENOCD_NOTE)
    return {"ide": ide, "free": free}


def kit_json():
    import board_template as bt
    pf = json.loads((ROOT / "site/data/portfolio.json").read_text())
    code = json.loads((ROOT / "site/data/code.json").read_text())["boards"]
    boards = json.loads((BENCH / "boards.json").read_text())["boards"]
    out = {}
    for b in boards:
        bid, kit = b["id"], b["kit"]
        p = next((dict(x) for x in pf["parts"] if x["board"] == kit), {})
        if not p.get("family") and bid in FAMILY_FALLBACK:
            p["family"] = FAMILY_FALLBACK[bid]
        folder = (code.get(bid) or {}).get("folder", "")
        ctx = dict(bid=bid, name=kit, p=p, folder=folder, dev="", recipe=None)
        opts = [o for o in bt.kit_options(None, ctx) if o["label"] != "Run the first example"]
        routes = (json.loads((ROOT / "data/routes.json").read_text()).get("boards") or {}).get(bid) or {}
        out[kit] = {"id": bid, "folder": folder, "options": opts, "tools": terminal_tools(bid, p.get("family", "")),
                    "install": routes.get("install_total", "")}
    return {"made_from": ["tools/board_template.kit_options", "site/data/portfolio.json", "site/data/code.json", "site/data/projects.json", "data/routes.json", "site/flash/bench/boards.json"],
            "demo": {"label": "Run this demo", "text": DEMO_TEXT, "text_nofolder": DEMO_TEXT_NOFOLDER},
            "boards": out}


# --------------------------------------------------------------------------------------------- the generated CSS
TOKENS = """
:root { --frl-red: #B42418; --frl-ember: #8E1C13; --frl-ink: #161312; --frl-stone: #6B625B; --frl-paper: #FAFAF7; --frl-tan: #F4EFE9; --frl-cream: #F7F3EC; --frl-line: #E2D8C9;
  --ok: #2F6B4F; --frl-font: "Barlow", "Helvetica Neue", Arial, sans-serif; --frl-code: ui-monospace, "SF Mono", Menlo, Consolas, monospace; --frl-ease: 180ms cubic-bezier(0.2, 0, 0, 1); }
.wrap { max-width: 1200px; margin: 0 auto; padding: 0 24px; }
"""
GAPS = """
/* the kit sits in a light card; its copy button and links follow the page */
#takehome .k-kit { margin: 0; }
#takehome .k-os button { color: var(--frl-ink); min-height: 0; } #takehome .k-os button[aria-pressed="true"] { color: #fff; }
#takehome .k-go button { color: #fff; min-height: 0; } #takehome .k-go button:hover { background: var(--frl-red); }
"""


def css_block():
    return "/*bench-chrome:css*/" + TOKENS + sc.CHROME_CSS + sk.KIT_CSS + GAPS + "/*/bench-chrome:css*/"


def kit_block(kj):
    """Take it home, two copy buttons: the exact terminal lines, and the assistant's sentence (ticked options, rebuilt by ui.js
    when the board or demo changes). Both follow the one Mac / Windows / Linux switch."""
    first = next(iter(kj["boards"].values()))
    rows = []
    for o in first["options"]:
        attrs = "".join(f' data-p-{k}="{sk.esc(v)}"' for k, v in o["texts"].items())
        on = o.get("locked") or o.get("on")
        rows.append(f'<label><input type="checkbox"{attrs}{" checked" if on else ""}{" disabled" if o.get("locked") else ""}>'
                    f'<span>{sk.esc(o["label"])}<small>{sk.esc(o["sub"])}</small></span></label>')
    osbar = ('<div class="k-os" role="group" aria-label="Your computer">'
             '<button type="button" data-os="mac" aria-pressed="true">Mac</button>'
             '<button type="button" data-os="win" aria-pressed="false">Windows</button>'
             '<button type="button" data-os="linux" aria-pressed="false">Linux</button></div>')
    html = (f'<div id="home-kit">{osbar}<div class="th-two">'
            '<div class="th-col" id="th-term"><h3>Copy for your terminal</h3>'
            '<p class="small">The exact lines, nothing else: clone, tools, build, flash, check.</p>'
            '<pre class="th-pre" id="th-term-pre"></pre>'
            '<div class="k-go"><button type="button" id="th-term-copy">Copy for your terminal</button></div>'
            '<p class="k-note" id="th-term-note"></p></div>'
            f'<div class="th-col k-kit" id="th-ai"><h3>Copy for your AI</h3>{"".join(rows)}'
            '<p class="k-prev-h">You will copy</p><p class="k-prev"></p>'
            '<div class="k-go"><button type="button" id="th-ai-copy">Copy for your AI</button></div>'
            '<p class="k-note">Paste it into Claude Code, Codex, Cursor or any assistant that can run commands. It needs more tokens than the terminal lines and does the setup for you.</p></div>'
            '</div>'
            '<details class="th-fold"><summary>Sizes, source and one change to try</summary>'
            '<p class="small" id="th-install"></p>'
            '<dl id="f-facts" class="recovery hidden" aria-label="This demo\'s source and build"></dl>'
            '<p class="small" id="f-change"></p>'
            f'<p class="small"><a href="{sk.REPO_URL.removesuffix(".git")}">The repository on GitHub</a></p></details></div>')
    return "<!--bench-chrome:kit-->" + html + "<!--/bench-chrome:kit-->"


def swap(text, name, new, kind):
    pat = (re.compile(rf"<!--bench-chrome:{name}-->.*?<!--/bench-chrome:{name}-->", re.S) if kind == "html"
           else re.compile(r"/\*bench-chrome:css\*/.*?/\*/bench-chrome:css\*/", re.S))
    if not pat.search(text):
        raise SystemExit(f"build_ui: marker {name} is missing from {PAGE.relative_to(ROOT)}")
    return pat.sub(lambda m: new, text, count=1)


def build():
    kj = kit_json()
    t = PAGE.read_text()
    t = swap(t, "header", "<!--bench-chrome:header-->" + rebase(sc.header(None, True)) + "<!--/bench-chrome:header-->", "html")
    t = swap(t, "footer", "<!--bench-chrome:footer-->" + rebase(sc.footer(None)) + "<!--/bench-chrome:footer-->", "html")
    t = swap(t, "css", css_block(), "css")
    t = swap(t, "kit", kit_block(kj), "html")
    return t, json.dumps(kj, indent=1, ensure_ascii=False) + "\n", json.dumps(demos_json(), indent=1, ensure_ascii=False) + "\n"


CHIP = BENCH.parent / "chip" / "index.html"   # the chip view page: same depth, same chrome blocks, its own body


def build_chip():
    """site/flash/chip/index.html with the current header, footer and chrome CSS (the page is hand-written otherwise)."""
    if not CHIP.exists():
        return None
    t = CHIP.read_text()
    t = swap(t, "header", "<!--bench-chrome:header-->" + rebase(sc.header(None, True)) + "<!--/bench-chrome:header-->", "html")
    t = swap(t, "footer", "<!--bench-chrome:footer-->" + rebase(sc.footer(None)) + "<!--/bench-chrome:footer-->", "html")
    return swap(t, "css", css_block(), "css")


def main():
    page, kit, demos = build()
    kit_path, demos_path = BENCH / "kit.json", BENCH / "demos.json"
    have = lambda p: p.read_text() if p.exists() else ""
    chip = build_chip()
    stale = [n for n, cur, new in (("index.html", PAGE.read_text(), page), ("kit.json", have(kit_path), kit), ("demos.json", have(demos_path), demos)) if cur != new]
    if chip is not None and CHIP.read_text() != chip:
        stale.append("chip/index.html")
    if "--check" in sys.argv:
        if stale:
            print("STALE: " + ", ".join(stale) + "; run python3 tools/webflash/build_ui.py")
            sys.exit(1)
        print("OK: the Digital Lab chrome, kit and demo list are current")
        return
    if chip is not None:
        CHIP.write_text(chip)
    PAGE.write_text(page); kit_path.write_text(kit); demos_path.write_text(demos)
    print("wrote site/flash/bench/index.html blocks, kit.json and demos.json" if stale else "already current")


if __name__ == "__main__":
    main()
