#!/usr/bin/env python3
"""Digital Lab recipe entries for the radio boards' demos (cc2755p10/radio-demos/build.sh output in prebuilt/<board>/radio-*/).
   entries(out_dir) copies each image into site/flash/bench/recipes/<board>/<id>/ and returns the recipe dicts; build_page.py's wireless()
   appends them, and `python3 tools/webflash/radio_recipes.py --merge` adds or replaces them in recipes.json without touching any other entry."""
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "site" / "flash" / "bench"
KITS = {"cc2755p10": "LP-EM-CC2755P10", "cc2340r5": "LP-EM-CC2340R5"}
PROMPT = ("Set up my TI LaunchPad with Fox Red Labs. Run git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git, "
          "then read fox-red-labs/START.md in full and follow it. Ask me before installing anything or writing to the board. It is an {kit}. {want}")
TEXT = {
    "blink": dict(title="Blink the red LED: set the on and off times", want="I want its red LED to blink at my own times.",
                  outcome="The board runs a blink program. Set the on and off times on this page; they are written into the program's settings block with the program, so no build is needed. The page reads a counter in the chip's RAM to prove it is running.",
                  see="The red LED (LED0) is on for the first time and off for the second, repeating.", kind="status"),
    "message": dict(title="Print “Hello from my LaunchPad” over the USB cable", want="I want it to print my own message over its USB serial port.",
                    outcome="The board prints your line once a second (or at the time you set), counting, on the probe's serial port at 115200 baud; the page reads it with Web Serial.",
                    see="“> Hello from my LaunchPad 1”, then 2, 3 and on, once a second.", kind="uart"),
    "button": dict(title="Buttons: see them on the LEDs and in the console", want="I want to read its two buttons.",
                   outcome="LED0 follows BTN-1 and LED1 follows BTN-2. The board prints the button states on the probe's serial port whenever one changes and once a second; a RAM word holds the press count.",
                   see="Hold BTN-1 and LED0 lights, hold BTN-2 and LED1 lights; the console line changes from up to down.", kind="uart"),
    "zigbee": dict(title="Zigbee on/off light node (TI's example)", want="I want to run a Zigbee light node.",
                   outcome="TI's Zigbee on/off light example, unchanged: the board starts the Zigbee stack as a relaying node and logs on the probe's serial port (TI's binary log format at 115200 baud). Joining a coordinator was not tested.",
                   see="The console fills with log frames within a second of the start; the stack waits for a network to join.", kind="uart"),
}


def entries(out_dir=OUT):
    recs = []
    for board, kit in KITS.items():
        for d in sorted((ROOT / "prebuilt" / board).glob("radio-*")):
            demo = d.name.split("-", 1)[1]
            if demo not in TEXT or not (d / "manifest.json").exists():
                continue
            m = json.loads((d / "manifest.json").read_text())
            t = TEXT[demo]
            dst = out_dir / "recipes" / board / d.name
            dst.mkdir(parents=True, exist_ok=True)
            for f in (d / f"{d.name}.hex", d / "manifest.json", d / "NOTICE.txt"):
                if f.exists():
                    shutil.copy2(f, dst / f.name)
            tested = "not tested on a board" if "not run on a board" in " ".join(m["revisions_tested"]) else "tested on " + m["revisions_tested"][0]
            if t["kind"] == "status":
                check = {"kind": "status", "addr": m["status"]["addr"], "magic": m["status"]["magic"], "word": "counter", "seconds": 4.0, "minRise": 3}
            elif demo == "message":
                check = {"kind": "uart", "message": "Hello from my LaunchPad", "baud": 115200, "seconds": 15.0}
            elif demo == "button":
                check = {"kind": "uart", "message": "buttons: BTN-1", "contains": True, "baud": 115200, "seconds": 15.0}
            else:
                check = {"kind": "uart", "message": "", "contains": True, "binary": True, "minBytes": 64, "baud": 115200, "seconds": 15.0}
            rebuild = m["source"]["rebuild"]
            rec = {
                "id": d.name, "board": board, "recipe": d.name, "project": "blink", "title": t["title"], "outcome": t["outcome"], "see": t["see"],
                "before": "The board needs the program on it once: the button below saves what is on it first, then writes it. Console: choose the probe's second serial port.",
                "check": check, "wiring": None, "image": None, "hex": f"recipes/{board}/{d.name}/{d.name}.hex", "sha256": m["image"]["sha256"], "bytes": None,
                "console": m.get("console"), "params": m.get("params"), "status": m.get("status"), "tested": tested,
                "manifest": f"recipes/{board}/{d.name}/manifest.json",
                "terminal": rebuild, "prompt": PROMPT.format(kit=kit, want=t["want"]),
                "source": f"TI's {m['source']['example'].split('/', 3)[-1]} example for the {kit}" + ("" if demo == "zigbee" else " with the Fox Red Labs demo program (cc2755p10/radio-demos/frl_demo.c)") + f"; {tested}.",
                "further": {"route": "build", "command": rebuild, "note": "",
                            "change": "Change the settings on this page: no build." if m.get("params") else "This is TI's example, unchanged: change it in TI's example in the SDK, then rebuild with the command above.",
                            "source": ["on_off_light.c in TI's examples/rtos/" + kit.replace("-", "_") + "/zigbee/onoff_light example, as copied into your project folder"] if demo == "zigbee" else ["cc2755p10/radio-demos/frl_demo.c"], "example": m["source"]["example"],
                            "built_with": f"SimpleLink Low Power F3 SDK {m['tools']['sdk']['version']}, SysConfig {m['tools']['sysconfig']['version']}, TI Arm Clang {m['tools']['compiler']['version']}",
                            "commit": m["source"]["repo_commit"][:12], "notice": f"recipes/{board}/{d.name}/NOTICE.txt", "license": ""},
            }
            recs.append(rec)
    return recs


if __name__ == "__main__":
    if "--merge" not in sys.argv:
        print(json.dumps(entries(Path("/dev/null")), indent=1)[:200]); sys.exit(0)
    p = OUT / "recipes.json"
    doc = json.loads(p.read_text())
    new = entries()
    keys = {(r["board"], r["id"]) for r in new}
    doc["recipes"] = [r for r in doc["recipes"] if (r["board"], r["id"]) not in keys] + new
    p.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
    print(f"merged {len(new)} radio demo recipes into {p.relative_to(ROOT)}")
