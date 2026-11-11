#!/usr/bin/env python3
"""Write the browser bench's data from the same sources the local tool uses.

  python3 tools/webflash/build_page.py          writes site/flash/bench/board.json, recipes.json and recipes/<id>/

board.json comes from data/routes.json (identity, writable flash, LEDs, console). recipes.json lists every
prebuilt image for the board (prebuilt/<board>/*/manifest.json) with its recipe text and check from
data/routes.json, its project's outcome and any wiring from site/data/projects.json, and the terminal command
and AI prompt for going further. recipes/<id>/ is a copy of each prebuilt image, manifest and notice. The bench
page (site/flash/bench/index.html) reads nothing else. The prebuilt images are not cleared for public release, so both
publish manifests leave site/flash/ out.
"""
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "site" / "flash" / "bench"
BOARD = "mspm0l1306"
PROMPT = ("Set up my TI LaunchPad with Fox Red Labs. Run git clone --depth 1 --filter=blob:none --sparse "
          "https://github.com/Fox-Red-Labs/fox-red-labs.git, then read fox-red-labs/START.md in full and follow it. "
          "Ask me before installing anything or writing to the board. It is an {kit}. {want}")
CLONE = "git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git\ncd fox-red-labs\n"


# What the page saves before it writes, what the write changes, and whether this page can put the copy back: one entry
# per board, shown beside the write button and in "Put back what was there" (persona review B01, 2026-10-15). A board
# whose way back needs TI's tools says so and names the procedure; "tested" is what has actually run on hardware.
PAGE_RESTORE = {
    "kind": "page",
    "saves": "the board's whole main flash, as one .json file in your Downloads folder, before the first write",
    "changes": "only the main-flash sectors the program needs; never the boot configuration",
    "back": "\u201cPut back what was there\u201d below writes that .json file back and reads it back to compare. It accepts only a copy saved from this same chip.",
    "tested": "Put back works on the LP-MSPM0L1306: 13.5 s in Chrome on a Mac, and 7.8 s after a write cut off part way, read back identical.",
}
PUT_BACK_RUNS = {"mspm0g5187": "Put back works on this board: 16.5 s in Chrome on a Mac, read back identical.",
                 "mspm0g3519": "Put back works on this board after a write cut off part way: 512 kB read back identical in 59.7 s."}
NO_WRITE = {"kind": "none", "saves": "nothing", "changes": "nothing: this page only reads this board",
            "back": "Nothing to put back.", "tested": ""}
VENDOR_RESTORE = {
    "cc27": {
        "kind": "vendor",
        "saves": "the main flash and both configuration areas, as three .bin files in your Downloads folder (<board>-<time>-main.bin, -ccfg.bin, -scfg.bin), and only if the board holds a different program",
        "changes": "erases the main flash (TI's security firmware at the top stays) and rewrites both configuration areas, CCFG and SCFG",
        "back": "This page cannot put those files back. TI's UniFlash can, with the repository's restore script.",
        "tool": "TI UniFlash 9.6.0 (its DSLite command line) on a Mac",
        "files": [["<board>-<time>-main.bin", "backup-flash-1.bin", "main flash, 0x0 to 0xE8000"],
                  ["<board>-<time>-ccfg.bin", "backup-ccfg-1.bin", "CCFG, 2 kB at 0x4E020000"],
                  ["<board>-<time>-scfg.bin", "scfg-sdk-default.bin", "SCFG, 1 kB at 0x4E040000; the script checks its CRC32"]],
        "steps": ["Put the three files in one folder and rename them as in the table.",
                  "In a copy of this repository, with UniFlash installed at /Applications/ti/uniflash_9.6.0, run: cc2755p10/restore.sh <that folder>",
                  "It erases the chip (keeping TI's security firmware), writes all three, reads the main flash and CCFG back, and prints RESTORED when they match."],
        "tested": "Not tested with files this page saved. The same steps worked on 2026-10-09 with files read by TI's DSLite.",
    },
    "cc23": {
        "kind": "vendor", "proven": True,
        "saves": "the main flash and the configuration area, as two .bin files in your Downloads folder (<board>-<time>-main.bin and -ccfg.bin), and only if the board holds a different program that starts; a board with no program has nothing to save",
        "changes": "erases the main flash and rewrites the configuration area (CCFG)",
        "back": "This page cannot put those files back. TI's UniFlash can, with the repository's put-back script.",
        "tool": "TI UniFlash 9.6.0 (its DSLite command line) on a Mac",
        "files": [["<board>-<time>-main.bin", "keep the name", "main flash, 512 kB from 0x0"],
                  ["<board>-<time>-ccfg.bin", "keep the name", "CCFG, 2 kB at 0x4E020000"]],
        "steps": ["Put the two files from one save in a folder of their own.",
                  "In a copy of this repository, with UniFlash installed at /Applications/ti/uniflash_9.6.0 and your probe's serial in cc2340r5/CC2340R5.ccxml, run: cc2340r5/restore-from-page.sh <that folder>",
                  "It merges the two files, erases the chip, writes them, reads main flash and CCFG back and prints RESTORED when they match. A program that keeps data in TI's NVS area (the top 4 kB; a Zigbee image keeps its network there) may write it as soon as it starts; the script reports those bytes rather than failing."],
        "tested": "Passed on 2026-10-16 with files this page saved: a Zigbee alarm image went back read-back identical (except 375 bytes it wrote to its own NVS area after starting) and answered Home Assistant without being paired again; the put-back took 30 s.",
    },
}


def wiring_for(project, board_id):
    """The wiring block of the project variant that ran on this board, or None (onboard parts only)."""
    for v in (project.get("recipe") or {}).get("variants", []):
        if board_id in v.get("boards", []) and v.get("status") == "tested" and v.get("wiring"):
            return v["wiring"]
    return None


PRESETS = [
    {"name": "Rainbow", "steps": [[1, 300], [3, 300], [2, 300], [6, 300], [4, 300], [5, 300]]},
    {"name": "Traffic light", "steps": [[2, 1500], [3, 500], [1, 1500]]},
    {"name": "Police", "steps": [[1, 120], [0, 80], [1, 120], [0, 80], [4, 120], [0, 80], [4, 120], [0, 280]]},
    {"name": "Heartbeat", "steps": [[1, 120], [0, 120], [1, 120], [0, 640]]},
    {"name": "Red, green, blue", "steps": [[1, 300], [2, 300], [4, 300], [0, 300]]},
]


def color_player(m, did, b):
    """The color player: a program that reads LED2's sequence from one flash sector the page rewrites."""
    t = m["table"]
    return {
        "id": did, "recipe": "color-player", "project": "blink",
        "title": "Pick LED2's colors and send them to the board",
        "outcome": "Choose a sequence of colors and times, press Send, and LED2 plays it about a second later; the page reads the LED pins to check it.",
        "see": "LED2 plays your colors in order, repeating.",
        "before": b.get("checks_before_flash", ""), "wiring": None,
        "check": {"kind": "colors", "seconds": 4.0, "tolMs": 40},
        "table": {**t, "gpio": b["leds"][0]["gpio_base"]}, "presets": PRESETS,
        "image": f"recipes/{did}/{m['image']['file']}", "manifest": f"recipes/{did}/manifest.json",
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The Fox Red Labs color player ({m['source']['file']}), built on TI's {Path(m['source']['example']).name} example "
                  f"from the MSPM0 SDK {m['tools']['sdk']['version']} with TI Arm Clang {m['tools']['compiler']['version']}. "
                  "The first send writes the program; after that each send rewrites only the one sector that holds your sequence. "
                  "You change the data the program runs, not the program itself.",
        "terminal": CLONE + f"./start.sh {BOARD} blink --pattern 100,100,100,100,100,700",
        "prompt": PROMPT.format(kit=b["kit"], want="I want LED2 to play my own color sequence, and to change it in plain English."),
    }


def thermometer(m, did, kit):
    """The chip's own temperature sensor, read live through the debugger (tools/webflash/build_sensor.py)."""
    th = m["thermometer"]
    return {
        "id": did, "recipe": "thermometer", "project": "blink",
        "title": "Live thermometer: the chip's own temperature",
        "outcome": "The chip measures its own temperature ten times a second; the page reads it through the USB cable and shows it on a dial.",
        "see": "A dial with the chip's temperature. Put a finger on the chip and watch it climb.",
        "before": "", "wiring": None,
        "check": {"kind": "thermo", **{k: th[k] for k in ("memres", "trim", "vref", "bits", "mv_per_c", "trim_c", "trim_refs", "trim_window_v", "typical_vtrim")}},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The Fox Red Labs thermometer ({m['source']['file']}), built on TI's gpio_toggle_output example for the {kit} "
                  f"from the MSPM0 SDK {m['tools']['mspm0_sdk']['version']}, with the ADC settings and the conversion to degrees C from "
                  "TI's adc12_internal_temp_sensor_mathacl example",
        "terminal": CLONE + "./start.sh " + m["board"] + " blink",
        "prompt": PROMPT.format(kit=kit, want="I want to read the chip's temperature sensor and print it."),
    }


def example_prompt(kit, m, name, src_file, board, key):
    return PROMPT.format(kit=kit, want=f"I want to change the {name} example (tools/webflash/player/{src_file}) and rebuild it with "
                                       f"python3 tools/webflash/build_examples.py {board} --only {key}, then check it on the board.")


def button(m, did, d):
    """Hold S2, the LED lights: the page reads the button pin and the LED pin through the debugger (build_examples.py)."""
    led, bt = m["led"], m["button"]
    return {
        "id": did, "recipe": "button", "project": "blink", "short": "Button to LED",
        "title": f"Button to LED: hold {bt['label'].split(' (')[0]}, {led['label'].split(' (')[0]} lights",
        "outcome": f"The program reads the {bt['label']} button and lights {led['label']} while it is held. The page reads the button pin and the LED pin through the debugger and shows them side by side.",
        "see": f"{led['label']} is lit while you hold {bt['label'].split(' (')[0]} and dark when you let go.",
        "before": "; ".join(d.get("jumpers", [])), "wiring": None,
        "check": {"kind": "button", "seconds": 25, "tolMs": 40,
                  "led": {**led, "activeHigh": m["active_high"]},
                  "button": {"label": bt["label"], "gpio": bt["gpio"], "bit": bt["bit"], "pressedLevel": bt["pressed_level"]}},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The Fox Red Labs button-to-LED program ({m['source']['file']}), built on TI's gpio_toggle_output example for the {d['kit']} "
                  f"from the MSPM0 SDK {m['tools']['mspm0_sdk']['version']}; the button pin is set up in the program with DriverLib (internal pull-up)",
        "prompt": example_prompt(d["kit"], m, "button-to-LED", "button.c", m["board"], "button"),
    }


def breathe(m, did, d):
    """A breathing LED: software PWM; the page reads the duty the program keeps in RAM and the LED pin (build_examples.py)."""
    led, br = m["led"], m["breathe"]
    return {
        "id": did, "recipe": "breathe", "project": "blink", "short": "Breathing LED",
        "title": f"Breathing LED: {led['label'].split(' (')[0]} fades up and down",
        "outcome": f"{led['label']} slowly brightens and dims, about every two seconds. The dimming is a 1 kHz on-off pattern whose on-time the program changes by 1 percent a step. The page reads the program's duty from RAM and the LED pin through the debugger and checks that the pin follows the duty.",
        "see": f"{led['label']} fades up and down, one breath about every two seconds.",
        "before": "; ".join(d.get("jumpers", [])), "wiring": None,
        "check": {"kind": "breathe", "seconds": 3, "tolMs": 40, "addr": br["addr"], "magic": br["magic"], "periodMs": br["period_ms"],
                  "led": {**led, "activeHigh": m["active_high"]}},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The Fox Red Labs breathing-LED program ({m['source']['file']}), built on TI's gpio_toggle_output example for the {d['kit']} "
                  f"from the MSPM0 SDK {m['tools']['mspm0_sdk']['version']}",
        "prompt": example_prompt(d["kit"], m, "breathing-LED", "breathe.c", m["board"], "breathe"),
    }


def digit(m, did, kit):
    """Draw a digit in the browser; BitNetMCU's cnn_64 on the chip reads it (tools/webflash/build_digit.py)."""
    d = m["digit"]
    return {
        "id": did, "recipe": "digit", "project": "handwritten-digits",
        "title": "Draw a digit: the chip reads it",
        "outcome": "An edge AI model (BitNetMCU's cnn_64, 1-bit and 4-bit weights) runs on the chip. You draw a digit here; the page sends it through the USB cable and the chip answers with the digit and how long it took.",
        "see": "Draw a digit from 0 to 9 with your mouse or finger. The chip's answer and its time appear beside it.",
        "before": "", "wiring": None,
        "check": {"kind": "digit", **{k: d[k] for k in ("addr", "magic")}},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"BitNetMCU (Tim Schneider, GPL-3.0) cnn_64 model and inference code, unchanged, in the Fox Red Labs mailbox program "
                  f"({m['source']['file']}) on TI's gpio_toggle_output example for the {kit}, MSPM0 SDK {m['tools']['mspm0_sdk']['version']}, "
                  f"TI Arm Clang {m['tools']['ticlang']['version']}",
        "terminal": CLONE + "./start.sh " + m["board"] + " blink",
        "prompt": PROMPT.format(kit=kit, want="I want to run an edge AI model that reads handwritten digits."),
    }


WIRE_PICTURES = {"mspm0g3519": "docs/arrivals/img/mspm0g3519-loops.png"}   # numbered top-down picture; wire 3 is this loop


def wire_picture(board, w):
    src = WIRE_PICTURES.get(board)
    if not src:
        return None
    (OUT / "boards").mkdir(exist_ok=True)
    shutil.copy2(ROOT / src, OUT / "boards" / f"{board}-wire.png")
    return {"image": f"flash/bench/boards/{board}-wire.png", "alt": "The board from above with three numbered wires; this step needs only wire 3, the pink one.",
            "pins": [[w["from"], w["to"], "the only wire for this step: number 3, pink, in the picture"]]}


def first_wire(m, did, kit):
    """One jumper closes a DAC-to-ADC loop the page sweeps through the debugger (tools/webflash/build_wire.py)."""
    w = m["wire"]
    return {
        "id": did, "recipe": "first-wire", "project": "blink",
        "title": "Your first wire: close a loop",
        "outcome": "The chip's DAC makes a voltage and its ADC measures it. One wire joins them; the page sweeps the DAC from 0 to full scale and plots what the ADC read against what it asked for.",
        "see": f"Connect one jumper wire from {w['from']} to {w['to']}. The measured line snaps onto the diagonal when the wire goes in.",
        "before": f"One female-to-female jumper wire, {w['from']} to {w['to']}. Unplug the USB cable while you place it.",
        "wiring": wire_picture(m["board"], w),
        "check": {"kind": "loop", **{k: w[k] for k in ("addr", "magic", "from", "to")}},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The Fox Red Labs first-wire loop ({m['source']['file']}) on TI's gpio_toggle_output example for the {kit}, with the DAC "
                  f"settings of TI's dac12_fixed_voltage_vref_internal example; MSPM0 SDK {m['tools']['mspm0_sdk']['version']}",
        "terminal": CLONE + "./start.sh " + m["board"] + " blink",
        "prompt": PROMPT.format(kit=kit, want="I want to drive the DAC and read it back with the ADC through a wire."),
    }


def controls(m, did, b):
    """The no-code controls (pin, timer, bus): one table the page writes, one status block it reads (build_controls.py)."""
    return {
        "id": did, "recipe": "controls", "project": "blink",
        "title": "Pin, timer and bus: set them from this page, no code",
        "outcome": "Add the I/O you want with the plus: a pin (read it, drive it), a timer (toggle it), PWM (a frequency and duty), or the I2C bus (who answers, or read a register). Each row shows the line of C it stands for. Send, and the program on the board applies the list and reports back; the page checks a toggle and a PWM pin by sampling them through the debugger. A plugged-in BoosterPack from the list brings its own rows.",
        "see": "The pin you picked is lit on the board's drawing with its setup; the LED toggles at your times or dims at your duty; the bus answers with the addresses that acknowledged, or the bytes read.",
        "before": "Nothing to change if the board is as it came. The bus uses PA0 and PA1 with the board's own pull-ups (J9 and J10); put your I2C part on J1 pins 9 and 10, 3.3 V and GND.",
        "check": {"kind": "controls", "seconds": 4.0, "tolMs": 40, "gpio": m["gpio"], "pwm_seconds": 1.5},
        "table": m["table"], "status": m["status"], "pins": m["pins"], "i2c": m["i2c"], "wiring": None,
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"TI's i2c_controller_rw_multibyte_fifo_poll example from the MSPM0 SDK {m['tools']['mspm0_sdk']['version']} with its main source replaced by our controls program "
                  f"(tools/webflash/player/controls.c), built with TI Arm Clang {m['tools']['ticlang']['version']}",
        "terminal": CLONE + "python3 tools/webflash/build_controls.py   # rebuilds the controls program; the page writes its table",
        "prompt": PROMPT.format(kit=b["kit"], want="I want to drive a pin, toggle it on a timer and read an I2C part, set from a page without writing code."),
    }


def out_of_box(m, did, b):
    """TI's out-of-box demo, driven from the page over the board's USB serial port (the role of TI's demo GUI)."""
    o = m["oob"]
    return {
        "id": did, "recipe": "out-of-box", "project": "out-of-box",
        "title": "TI's out-of-box demo, live: light sensor and thermometer",
        "outcome": "The demo every board ships with. The page talks to it over the USB cable: switch on the light sensor or the thermometer and watch the readings live, or set how fast the LED blinks.",
        "see": "A live reading and chart; LED2 follows the light level, or turns red or blue as the board warms or cools by 2 degrees.",
        "before": b["console"]["jumpers"] + ".", "wiring": None,
        "check": {"kind": "oob", "baud": o["baud"], **o},
        "image": f"recipes/{did}/{m['image']['file']}", "manifest": f"recipes/{did}/manifest.json",
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"TI's out-of-box demo ({m['source']['example']}) from the MSPM0 SDK {m['tools']['sdk']['version']}, unchanged; the page speaks its MessagePack messages over the board's serial port at {o['baud']} baud. Boards ship with an earlier build of the same demo",
        "terminal": CLONE + f"./start.sh {BOARD} blink",
        "prompt": PROMPT.format(kit=b["kit"], want="I want to change TI's out-of-box demo: for example, make the light sensor drive the LED differently."),
    }


def coremark_table():
    """The best measured CoreMark per microcontroller board (data/results/*.csv), for the speed test's comparison."""
    import csv
    boards = {r["board"]: r for r in csv.DictReader((ROOT / "data" / "boards.csv").open())}
    best = {}
    for f in sorted((ROOT / "data" / "results").glob("*.csv")):
        for r in csv.DictReader(f.open()):
            if r.get("test") != "coremark" or r.get("status") != "measured":
                continue
            try:
                v = float(r["value"])
            except (TypeError, ValueError):
                continue
            bd = boards.get(r["board"], {})
            if "linux" in (bd.get("classes") or ""):
                continue
            if r["board"] not in best or v > best[r["board"]]["score"]:
                best[r["board"]] = {"board": r["board"], "name": bd.get("name") or r["board"], "vendor": bd.get("vendor", ""),
                                    "core": bd.get("core", ""), "mhz": r.get("clock_mhz") or bd.get("clock_mhz", ""),
                                    "score": round(v, 1), "source": str(f.relative_to(ROOT))}
    return sorted(best.values(), key=lambda x: -x["score"])


def speed_test(m, did, b):
    """CoreMark on the board: the score comes back from the chip's RAM through the debugger."""
    c = m["coremark"]
    return {
        "id": did, "recipe": "speed-test", "project": "speed-test",
        "title": "Speed test: run CoreMark and compare boards",
        "outcome": "Runs EEMBC's CoreMark on the chip for about 13 s, reads the score back from its memory through the debugger, and shows it beside every board we have measured.",
        "see": f"A CoreMark score near {c['expect_score']:g} at {c['clock_mhz']} MHz, and where this chip sits among the boards we have measured.",
        "before": "", "wiring": None,
        "check": {"kind": "coremark", "seconds": 30, **c},
        "compare": coremark_table(),
        "image": f"recipes/{did}/{m['image']['file']}", "manifest": f"recipes/{did}/manifest.json",
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": "EEMBC CoreMark 1.0 with our bare-metal port (mspm0l1306/bench/coremark), TI Arm Clang 5.1.1.LTS at -O3, 32 MHz; the comparison is the best measured score per board in data/results/",
        "terminal": CLONE + "./start.sh mspm0l1306 speed-test",
        "prompt": PROMPT.format(kit=b["kit"], want="I want to run CoreMark on it and compare the score with other boards."),
    }


def copy_recipe(man_path, m, dest):
    dest.mkdir(parents=True)
    for f in ("manifest.json", m["image"]["file"], "NOTICE.txt"):
        shutil.copy2(man_path.parent / f, dest / f)


def l1306():
    """The LP-MSPM0L1306: board data from data/routes.json, every prebuilt recipe in prebuilt/mspm0l1306/."""
    b = json.loads((ROOT / "data" / "routes.json").read_text())["boards"][BOARD]
    projects = {p["slug"]: p for p in json.loads((ROOT / "site" / "data" / "projects.json").read_text())["projects"]}
    ident = b["target"]["identity"]
    board = {
        "id": BOARD, "kit": b["kit"], "printed": b["printed"], "programmable": True,
        "identity": {"addr": ident["addr"], "deviceid": ident["deviceid"], "deviceidMask": ident["deviceid_mask"]},
        "flash": {"start": b["target"]["writable"][0]["start"], "end": b["target"]["writable"][0]["end"]},
        "gpio": b["gpio_offsets"],
        "console": {"baud": b["console"]["baud"], "jumpers": b["console"]["jumpers"]},
        "ram": 4096, "ramStart": "0x20000000", "cmdbyten": "0xFF",   # 4 KB SRAM at 0x20000000 (TI's linker file); 64-bit program without ECC (TI's L1306 flash examples)
        "picture": "boards/mspm0l1306.png", "recovery": PAGE_RESTORE,
    }
    shutil.copy2(ROOT / "mspm0l1306" / "img" / "board-tour.png", OUT / "boards" / "mspm0l1306.png")
    shutil.copy2(ROOT / "mspm0l1306" / "img" / "header-positions.json", OUT / "boards" / "mspm0l1306.pins.json")   # where each header hole sits on that picture (the controls card lights pins with it)
    recipes = []
    for man_path in sorted((ROOT / "prebuilt" / BOARD).glob("*/manifest.json")):
        m = json.loads(man_path.read_text())
        did = man_path.parent.name
        dest = OUT / "recipes" / BOARD / did
        copy_recipe(man_path, m, dest)
        if m["recipe"] == "out-of-box":
            rec = out_of_box(m, did, b)
        elif m["recipe"] == "thermometer":
            rec = thermometer(m, did, b["kit"])
        elif m["recipe"] == "digit":
            rec = ai_recipes.digit_recipe(m, did, b["kit"], PROMPT, CLONE)
        elif m["recipe"] == "occupancy":
            rec = ai_recipes.occupancy(m, did, b["kit"], PROMPT, CLONE)
        elif m["recipe"] == "speed-test":
            rec = speed_test(m, did, b)
        elif m["recipe"] == "controls":
            rec = controls(m, did, b)
        elif m["recipe"] == "color-player":
            rec = color_player(m, did, b)
            rec["check"]["activeHigh"] = True
            for c in rec["table"]["pins"].values():
                c.setdefault("gpio", rec["table"]["gpio"])
        else:
            r = b["recipes"][m["recipe"]]
            proj = projects.get(r["project"], {})
            v = r.get("verify") or {}
            if v.get("kind") == "uart":
                check = {"kind": "uart", "message": m["message"], "baud": b["console"]["baud"], "seconds": max(15.0, v.get("seconds", 4.0))}
                see = f"The board prints \u201c> {m['message']} 1\u201d, then 2, 3 and on, once a second, over its USB cable."
                before = b["console"]["jumpers"] + "."
                job = f"{r['project']} --message \"Your words here\""
                want = "I want it to print my own message."
            else:
                led = next(x for x in b["leds"] if x["label"] == r["led"])
                check = {"kind": "pattern", "pattern": m["pattern_ms"], "seconds": v.get("seconds", 4.0), "tolMs": v.get("tolerance_ms", 40),
                         "led": {"label": led["label"], "gpio": led["gpio_base"], "bit": led["bit"], "activeHigh": True}}
                see = f"{led['label'].split(' (')[0]} flashes: " + ", ".join(
                    f"{'on' if i % 2 == 0 else 'off'} {ms} ms" for i, ms in enumerate(m["pattern_ms"])) + ", repeating."
                before = b.get("checks_before_flash", "")
                job = "blink --pattern " + ",".join(map(str, m["pattern_ms"]))   # the demo it sits under
                want = "I want the LED to flash in my own pattern."
            rec = {
                "id": did, "recipe": m["recipe"], "project": r["project"],
                "title": (f"Print \u201c{m['message']}\u201d over the USB cable" if v.get("kind") == "uart"
                          else f"Blink LED2 red: {', '.join(map(str, m['pattern_ms']))} ms"),
                "outcome": ("TI's serial example, written to the board from this page. It prints the line once a second, counting; the page reads the board's USB serial port to check it."
                            if v.get("kind") == "uart" else
                            "TI's LED example, written to the board from this page. The page then times LED2 through the board's debugger to check it."),
                "see": see, "before": before, "wiring": wiring_for(proj, BOARD), "check": check,
                "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
                "source": f"TI's {Path(m['source']['example']).name} example from the MSPM0 SDK {m['tools']['sdk']['version']}, "
                          f"with one line changed ({m['source']['edited_line']}), built with TI Arm Clang {m['tools']['compiler']['version']}",
                "terminal": CLONE + f"./start.sh {BOARD} {job}",
                "prompt": PROMPT.format(kit=b["kit"], want=want),
            }
        rec["board"] = BOARD
        rec["image"] = f"recipes/{BOARD}/{did}/{m['image']['file']}"
        rec["manifest"] = f"recipes/{BOARD}/{did}/manifest.json"
        recipes.append(rec)
    return board, recipes


# Boards enabled for programming. Recovery records distinguish hardware-tested support from a new board build.
DRAFT_PROGRAMMABLE = {"mspm0g3507", "mspm0g3519", "mspm0g5187", "mspm0c1104"}
DRAFT_NOTE = {
    "mspm0g3507": "Blink and color-player firmware built for this board. Browser flashing and restore on the G3507 still need a hardware check.",
    "mspm33c321a": "Recognized. This page cannot program it yet.",
    "am13e230": "Recognized (prototype silicon). This page cannot program it yet.",
}


# Cortex-M33 boards: facts from site/flash/bench/boards-draft/<id>.json (SDK headers and the one-sector proof);
# images from tools/webflash/build_m33.py.
M33 = {
    "mspm33c321a": {"semaphore": {"request": "0x50047800", "status": "0x50047808", "clear": "0x50047804", "held": "0xC0010101"},
                    "protectAtRest": {"a": "0xFFFFFFFF", "b": "0x0FFFFFFF"}, "alias": "0x10000000", "ram": None},
    "am13e230": {"semaphore": {"request": "0x40047800", "status": "0x40047808", "clear": "0x40047804", "held": "0xC0010100"},
                 "protectAtRest": {"a": "0xFFFFFFFF", "b": "0x00000FFF"}, "alias": None, "ram": None},
}


def m33(bid):
    d = json.loads((OUT / "boards-draft" / f"{bid}.json").read_text())
    x = M33[bid]
    i = d["identity"]
    # Not offered yet (2026-10-16): the page writes and reads back both boards, but the written program has not started
    # cleanly after the page's reset; programmable flips to True once a full run (write, start, LED check, put back) passes.
    # The MSPM33 passed a full run on 2026-10-16 (write, AIRCR start, LED timed, put back). The AM13 is not offered
    # until it passes the same run (its first try left the LED unchanged; the board is on other work).
    proven = bid == "mspm33c321a"
    board = {"id": bid, "kit": d["kit"], "printed": d["kit"], "programmable": proven,
             "note": "" if proven else "Recognized. The page can write this board's flash, but starting the program afterwards is not proven yet, so it does not offer a write.",
             "identity": {"addr": i["addr"], "deviceid": i["deviceid"], "deviceidMask": i["deviceidMask"]},
             "flash": {"start": d["flash"]["start"], "end": d["flash"]["end"]}, "gpio": {"dout": d["gpio"]["dout"], "doe": d["gpio"]["doe"]},
             "cmdbyten": "0x3FFFF", "cpuss": None, "reset": "aircr",   # the Arm system reset request (see mspm0-dap.js resetRun)
             "fc": {"base": "0x40042000", "sector": 2048, "word": 16, "semaphore": x["semaphore"], "protectAtRest": x["protectAtRest"]},
             "recovery": {**PAGE_RESTORE, "tested": "Put back works on the LP-MSPM33C321A: write, start, LED check, then put back."} if proven else NO_WRITE}
    if x["alias"]:
        board["alias"] = x["alias"]
    recipes = []
    for man_path in sorted((ROOT / "prebuilt" / bid).glob("*/manifest.json")):
        m = json.loads(man_path.read_text())
        did = man_path.parent.name
        copy_recipe(man_path, m, OUT / "recipes" / bid / did)
        led = m["led"]
        recipes.append({"id": did, "board": bid, "recipe": m["recipe"], "project": "blink",
                        "image": f"recipes/{bid}/{did}/{m['image']['file']}", "manifest": f"recipes/{bid}/{did}/manifest.json",
                        "sha256": m["image"]["sha256"], "bytes": sum(sg["bytes"] for sg in m["image"]["segments"]), "wiring": None, "before": "",
                        "title": f"Blink {led['label'].split(' (')[0]}: {m['pattern_ms'][0]} ms on, {m['pattern_ms'][1]} ms off",
                        "outcome": "TI's LED example, written to the board from this page. The page then times the LED through the board's debugger to check it.",
                        "see": f"{led['label']} toggles every {m['pattern_ms'][0]} ms.",
                        "check": {"kind": "pattern", "pattern": m["pattern_ms"], "seconds": 4.0, "tolMs": 40, "led": {**led, "activeHigh": m["active_high"]}},
                        "source": f"TI's {Path(m['source']['example']).name} example for the {d['kit']} ({m['tools']['sdk']}), one line changed",
                        "terminal": CLONE + f"./{bid}/blink.sh {m['pattern_ms'][0]}", "prompt": PROMPT.format(kit=d["kit"], want="I want to blink its LED at my own rate.")})
    return board, recipes


def draft(bid):
    d = json.loads((OUT / "boards-draft" / f"{bid}.json").read_text())
    i = d["identity"]
    board = {"id": bid, "kit": d["kit"], "printed": d.get("printed", d["kit"]), "programmable": bid in DRAFT_PROGRAMMABLE,
             "identity": {"addr": i["addr"], "deviceid": i["deviceid"], "deviceidMask": i.get("deviceid_mask") or i.get("deviceidMask")},
             "note": DRAFT_NOTE.get(bid, ""), "recovery": NO_WRITE}
    recipes = []
    for key in ("userid", "useridMask"):
        if key in i:
            board["identity"][key] = i[key]
    if not board["programmable"]:
        return board, recipes
    board["recovery"] = {**PAGE_RESTORE, "tested": PUT_BACK_RUNS.get(bid, f"Put back from this page: no run recorded on the {d['kit']} yet. The same code passed on the LP-MSPM0L1306 and LP-MSPM0G5187.")}
    if bid == "mspm0g3507":
        board["recovery"]["proven"] = False
    board.update({"flash": {"start": d["flash"]["start"], "end": d["flash"]["end"]}, "gpio": d["gpio_offsets"],
                  "ram": d["ram"]["bytes"], "ramStart": d["ram"]["start"], "cmdbyten": d["cmdbyten"]})
    for man_path in sorted((ROOT / "prebuilt" / bid).glob("*/manifest.json")):
        m = json.loads(man_path.read_text())
        did = man_path.parent.name
        if m["recipe"] == "color-player" and len(m["table"]["pins"]) < 3:
            continue                                   # one-colour LED: the blink covers it
        copy_recipe(man_path, m, OUT / "recipes" / bid / did)
        common = {"id": did, "board": bid, "recipe": m["recipe"], "image": f"recipes/{bid}/{did}/{m['image']['file']}",
                  "manifest": f"recipes/{bid}/{did}/manifest.json", "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
                  "before": "; ".join(d.get("jumpers", [])), "wiring": None,
                  "terminal": CLONE + f"./start.sh {bid} blink", "prompt": PROMPT.format(kit=d["kit"], want="I want to blink its LED in my own pattern.")}
        if m["recipe"] == "thermometer":
            recipes.append({**common, **thermometer(m, did, d["kit"]), "board": bid, "image": common["image"], "manifest": common["manifest"]})
        elif m["recipe"] == "digit":
            recipes.append({**common, **ai_recipes.digit_recipe(m, did, d["kit"], PROMPT, CLONE), "board": bid, "image": common["image"], "manifest": common["manifest"]})
        elif m["recipe"] == "occupancy":
            recipes.append({**common, **ai_recipes.occupancy(m, did, d["kit"], PROMPT, CLONE), "board": bid, "image": common["image"], "manifest": common["manifest"]})
        elif m["recipe"] == "first-wire":
            recipes.append({**common, **first_wire(m, did, d["kit"]), "board": bid, "image": common["image"], "manifest": common["manifest"]})
        elif m["recipe"] in ("button", "breathe"):
            recipes.append({**common, **(button if m["recipe"] == "button" else breathe)(m, did, d), "board": bid, "image": common["image"], "manifest": common["manifest"]})
        elif m["recipe"] == "color-player":
            t = m["table"]
            recipes.append({**common, "project": "blink", "title": "Pick the RGB LED's colors and send them to the board",
                            "outcome": "Choose colors and times, press Send, and the LED plays them about a second later; the page reads the LED pins to check it.",
                            "see": "The RGB LED plays your colors in order, repeating.", "check": {"kind": "colors", "seconds": 4.0, "tolMs": 40},
                            "table": {**t, "active_high": t.get("active_high", True)}, "presets": PRESETS,
                            "source": f"The Fox Red Labs color player, built on TI's gpio_toggle_output example for the {d['kit']}"})
        else:
            led = m["led"]
            extra = {"short": m.get("short") or "Blink: two flashes"}
            if m.get("title"):
                extra["prompt"] = example_prompt(d["kit"], m, m["short"], "blink.c", m["board"], did.split("-", 1)[1])
            recipes.append({**common, **extra, "project": "blink", "title": m.get("title") or f"Blink {led['label'].split(' (')[0]}: {', '.join(map(str, m['pattern_ms']))} ms",
                            "outcome": m.get("outcome") or "TI's LED example, written to the board from this page. The page then times the LED through the board's debugger to check it.",
                            "see": m.get("see") or f"{led['label']} flashes twice, then pauses.",
                            "check": {"kind": "pattern", "pattern": m["pattern_ms"], "seconds": m.get("seconds", 4.0), "tolMs": 40,
                                      "led": {**led, "activeHigh": d.get("active_high", True)}},
                            "source": f"TI's gpio_toggle_output example for the {d['kit']} with the pattern changed"})
    return board, recipes


# Wireless boards, identified by TI's configuration access port (CFG-AP) after the dormant-port wake. DEVICEIDs are the ones
# TI's CCStudio script cc23xx_util.gel checks (IsCC23x0R5, IsCC27xx_1M), and the CC35X1's as read on our board (2026-10-14).
LIGHT_UUID = "8a570001-4724-4979-ab46-406d217495e5"
WIRELESS = [
    {"id": "cc2755p10", "kit": "LP-EM-CC2755P10", "ap": 1, "deviceids": ["0x0BB9802F"], "light": True, "saci": "cc27",
     "note": "Recognized. This page puts small programs on it (blink, a console message, the buttons, a Zigbee light, a Bluetooth light), reads its console and puts back what was on it."},
    {"id": "cc2340r5", "kit": "LP-EM-CC2340R5", "ap": 1, "deviceids": ["0x0BB8402F"], "light": True, "saci": "cc23",
     # 0x0BBAE02F is the CC2340R53 (TI's cc23xx_util.gel): its own predicted entry in boards-predicted.json, not this one (fixed 2026-10-17)
     "note": "Recognized. This page puts small programs on it (blink, a console message, the buttons, a Zigbee light, a Bluetooth light), reads its console and puts back what was on it."},
    {"id": "cc35x1", "kit": "LP-EM-CC35X1", "ap": 0, "deviceids": ["0x0BB9602F"], "light": False,
     "note": "Recognized. This page cannot write this board's flash: its code lives in a separate memory chip that only TI's own tool writes. With TI's over-the-air example on the board, the page updates it over Wi-Fi from your own server; the one-time Toolbox step is shown below.",
     # Route C (2026-10-22): recognise, show the console, give the exact Toolbox command.
     # Never here: activation, fuse, rollback or RMA commands.
     "console": json.loads(json.dumps({"baud": 115200, "port": "the second serial port of the XDS110 (on a Mac /dev/cu.usbmodem<probe serial>1)", "baudNote": "TI's SDK examples print at 115200; the Wi-Fi CSI program prints at 921600"})),
     "toolbox": {"command": "./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <absolute path to tool_settings.json>",
                 "from": "the SimpleLink Wi-Fi Toolbox folder (<Toolbox>)",
                 "before": "Build and sign the images first: the SDK's make, then the Toolbox image builder, which writes tool_settings.json (cc35x1/README.md steps 1 to 4). The board needs a debugger under it (LP-XDS110ET or LP-XDS110, TGT VDD on EXT) and a second USB-C cable for power.",
                 "after": "The Toolbox does not reset the board: press S1 or run xds110reset -s <probe serial>.",
                 "note": "This page does not write this board. Run the command on your computer."}},
]


def recovery_for_wireless(w):
    """The way back for a board the page writes through TI's SACI mailbox, with the board's own names in the files."""
    if not w.get("saci"):
        return NO_WRITE
    r = json.loads(json.dumps(VENDOR_RESTORE[w["saci"]]).replace("<board>", w["id"]))
    return r


def wireless():
    boards, recipes = [], []
    for w in WIRELESS:
        boards.append({"id": w["id"], "kit": w["kit"], "printed": w["kit"], "programmable": False, "note": w["note"], "saci": w.get("saci", False),
                       "recovery": recovery_for_wireless(w),
                       **({"console": w["console"], "toolbox": w["toolbox"]} if w.get("toolbox") else {}),
                       "identity": {"kind": "cfgap", "ap": w["ap"], "deviceids": w["deviceids"], "deviceidMask": "0x0FFFFFFF",
                                    **({"deviceidNames": w["deviceidNames"]} if w.get("deviceidNames") else {})}})
        hexes = sorted((ROOT / "prebuilt" / w["id"] / "ble-light").glob("*.hex"))
        if not (w["light"] and hexes):
            continue
        m = json.loads((ROOT / "prebuilt" / w["id"] / "ble-light" / "manifest.json").read_text())
        dst = OUT / "recipes" / w["id"] / "ble-light"; dst.mkdir(parents=True, exist_ok=True)
        shutil.copy2(hexes[0], dst / hexes[0].name)
        for extra in ("manifest.json", "NOTICE.txt"):
            if (hexes[0].parent / extra).exists():
                shutil.copy2(hexes[0].parent / extra, dst / extra)
        ccxml = {"cc2755p10": "cc2755p10/CC2755P10.ccxml", "cc2340r5": "cc2340r5/CC2340R5.ccxml"}[w["id"]]
        recipes.append({"id": "ble-light", "board": w["id"], "recipe": "ble-light", "project": "blink",
                        "title": "Bluetooth light: set its colors from this page, no cable",
                        "outcome": "The board runs our light program; the page finds it over Bluetooth and sets its red and green LEDs, or plays a pattern.",
                        "see": "The LEDs change a moment after you tap a color or a pattern.",
                        "before": "The board needs the light program on it once: the button below puts it there." if w.get("saci") else "The board needs the light program on it once. Until the page can write it, run the command below on a computer with TI's UniFlash.",
                        "check": {"kind": "ble", "service": LIGHT_UUID}, "wiring": None,
                        "image": None, "hex": f"recipes/{w['id']}/ble-light/{hexes[0].name}" if w.get("saci") else None, "sha256": m["image"]["sha256"] if "image" in m else "", "bytes": None,
                        "terminal": ("# assembled: flashes the light program with TI's UniFlash command-line tool (DSLite); "
                                     "set the probe serial in the .ccxml first\n"
                                     f"/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c {ccxml} -e -f -v -u prebuilt/{w['id']}/ble-light/{hexes[0].name}"),
                        "prompt": PROMPT.format(kit=w["kit"], want="I want its LEDs controlled over Bluetooth from a web page."),
                        "source": f"TI's basic_ble example for the {w['kit']} with the Fox Red Labs light service (cc2755p10/ble-light/)"})
    return boards, recipes


# "Go further" for each demo (persona review B04, 2026-10-15): the demo's own source, how its image was built, the
# tools and their versions, its notices, how it is checked and one small change, from the image's manifest. A guided
# command is given only where ./start.sh runs that job without an assistant (data/routes.json); elsewhere the page
# says so and gives the exact command that built the image instead of another demo's.
GUIDED = {("mspm0l1306", "blink-pattern"): ("./start.sh mspm0l1306 blink --pattern 100,100,100,100,100,700",
                                            "Change the numbers after --pattern (on, off, on, off, in milliseconds) and run the command again."),
          ("mspm0l1306", "print-message"): ('./start.sh mspm0l1306 print-a-message --message "Your words here"',
                                            "Change the words after --message and run the command again.")}
BOARD_SCRIPT = {"mspm33c321a": ("./mspm33c321a/blink.sh 400", "Change 400 (milliseconds per toggle) and run it again."),
                "am13e230": ("./am13e230/blink.sh 400", "Change 400 (milliseconds per toggle) and run it again.")}
CHANGE = {
    "thermo": "In tools/webflash/player/thermometer.c, change 100u in delay_cycles(100u * CYCLES_PER_MS) (line 67) to 500u: the chip then measures twice a second instead of ten times, and the dial steps more slowly.",
    "colors": "Change the colors and times on this page: no build. To change the program, edit the source file below.",
    "controls": "Change the pin, the times or the bus request on this page: no build. The C shown under the controls is what the program does; to add a control, edit tools/webflash/player/controls.c and rebuild with the command above.",
    "digit": "Draw on the pad: the model and its weights are BitNetMCU's, unchanged. No small change to the program is suggested yet.",
    "coremark": "Build it at another optimisation level (O2 instead of O3: mspm0l1306/bench/coremark/build.sh O2 32) and compare the score.",
    "oob": "This is TI's demo, unchanged: change it in TI's example in the SDK, then rebuild with the command above.",
    "loop": "In tools/webflash/player/wire.c, change the 2u in delay_cycles(2u * CYCLES_PER_MS) (line 92), rebuild, and watch whether the sweep line still settles.",
    "ble": "Edit frl_light.c (the Bluetooth light service), rebuild with the command above, then press the button on this page again.",
    "pattern": "Change the pattern in the source file, rebuild with the command above and send it from this page again.",
    "button": "In tools/webflash/player/button.c, change the argument of led(...) so the LED is lit until you press S2 instead of while you hold it, rebuild with the command above and flash it again.",
    "breathe": "In tools/webflash/player/breathe.c, change MS_PER_STEP from 10u to 25u (one breath then takes five seconds), rebuild with the command above and flash it again.",
    "uart": "Change the message in the source file, rebuild with the command above and send it from this page again.",
}
# Folders a build reads beyond tools/ and the folders its command and source files name (build_digit.py, build_occ.py).
KIND_DIRS = {"digit": ["ports/bitnetmcu", "third-party/bitnetmcu"], "occ": ["mspm0g3507"]}


def sparse_dirs(kind, cmd, files):
    """What `git sparse-checkout add` needs before cmd runs in the clone README.md gives (top-level files only)."""
    dirs = {"tools", *KIND_DIRS.get(kind, [])}
    for p in cmd.split() + [f.split(" ", 1)[0] for f in files]:
        top = p.lstrip("./").split("/", 1)[0] if "/" in p else ""
        if top and top != "tools" and (ROOT / top).is_dir():
            dirs.add(top)
    return sorted(dirs)


TOOL_NAMES = {"mspm0_sdk": "MSPM0 SDK", "sdk": "SDK", "sysconfig": "SysConfig", "ticlang": "TI Arm Clang", "compiler": "TI Arm Clang",
              "make": "make", "openocd": "OpenOCD"}


def further(rec, m):
    """The go-further block for one bench demo, from its manifest m (None when there is none)."""
    m = m or {}
    src = m.get("source") or {}
    kind = rec["check"]["kind"]
    sdk = {"mspm0": "MSPM0 SDK", "mspm33": "MSPM33 SDK", "am13e2": "AM13E230x SDK", "cc": "SimpleLink Low Power F3 SDK"}
    sdk_name = next((v for k, v in sdk.items() if rec["board"].startswith(k)), "SDK")
    parts = []
    for k, v in (m.get("tools") or {}).items():
        ver = v.get("version") if isinstance(v, dict) else v     # newer manifests: {"version": ...}; the M33 ones: the install folder name
        if not ver:
            continue
        name = sdk_name if k in ("sdk", "mspm0_sdk") else TOOL_NAMES.get(k, k)
        parts.append(f"{name} {ver}" if isinstance(v, dict) else f"{name} ({ver})")
    tools = ", ".join(parts)
    path = src.get("file") or ""
    files = ([path] if "/" in path else ([f"{path} in TI's {src['example']} example, as copied into your project folder"] if path else [])) + list(src.get("added") or [])
    if kind == "ble":
        files = [f"cc2755p10/ble-light/{f}" for f in src.get("added") or []]
    elif rec["recipe"].startswith("radio-"):         # radio_recipes.py: the added files sit beside cc2755p10/radio-demos/build.sh
        files = [f if "/" in f or " " in f else f"cc2755p10/radio-demos/{f}" for f in files]
    guided = GUIDED.get((rec["board"], rec["recipe"]))
    script = BOARD_SCRIPT.get(rec["board"]) if rec["recipe"] == "blink-pattern" else None
    if guided:
        cmd = guided[0]
        if rec["recipe"] == "blink-pattern" and rec["check"].get("pattern"):   # the command makes the demo it sits under
            cmd = cmd.split(" --pattern ")[0] + " --pattern " + ",".join(map(str, rec["check"]["pattern"]))
        route, cmd, change = "guided", CLONE + cmd, guided[1]
    elif script:
        route, cmd, change = "script", script[0], script[1]
    else:
        route, cmd, change = "build", src.get("rebuild") or "", CHANGE.get(kind, "")
    note = ""
    if " (" in cmd:                                   # "build.sh O3 32 (it empties build-O3-32/ first: ...)": the command, then its note
        cmd, note = cmd.split(" (", 1)
        note = note.rstrip(")")
    notice = rec["manifest"].rsplit("/", 1)[0] + "/NOTICE.txt" if rec.get("manifest") else None
    img = (m.get("image") or {}).get("file")
    built = f"prebuilt/{rec['board']}/{rec['id']}/{img}" if img and (ROOT / "prebuilt" / rec["board"] / rec["id"] / img).exists() else ""
    return {"route": route, "command": cmd, "note": note, "change": change, "source": files, "example": src.get("example", ""),
            "dirs": sparse_dirs(kind, cmd, files) if route != "guided" else [], "image": built,
            "frl": not (json.loads((ROOT / "data" / "routes.json").read_text())["boards"].get(rec["board"], {"target": {"write_blocked": "no route"}}).get("target", {}).get("write_blocked")),   # frl writes images to this board (a route with write_blocked does not)
            "built_with": tools, "commit": (src.get("repo_commit") or "")[:12], "notice": notice,
            "license": "GPL-3.0 (BitNetMCU, third-party/bitnetmcu)" if kind == "digit" else ""}


from board_catalog import catalog
import ai_recipes


def resources(bid):
    """What the bench's resources strip shows the moment a board is found (2026-10-17): the chip's
    clock, flash and RAM from data/boards.csv, and the operating-system builds measured for the board (the same
    memory-breakdown file the board page's code size section reads), each with flash and RAM used."""
    import csv
    out = {}
    with (ROOT / "data" / "boards.csv").open() as f:
        for row in csv.DictReader(f):
            if row["board"] == bid:
                for k in ("clock_mhz", "flash_kb", "ram_kb"):
                    if row.get(k):
                        out[k] = float(row[k]) if "." in row[k] else int(row[k])
    bj = ROOT / "site" / "data" / "boards" / f"{bid}.json"
    if bj.exists():
        board = json.loads(bj.read_text())
        src = ROOT / board.get("memory_source", "")
        if board.get("memory_source") and src.is_file():
            mem = json.loads(src.read_text())
            builds = []
            for b in mem["builds"]:
                region = ((b.get("check") or {}).get("flash_region") or {}).get("length") or mem["flash_total"]
                flash = sum(v for comp, v in b["flash"].items() if "CCFG" not in comp)
                ram = sum(b["ram"].values())
                builds.append({"id": b["id"], "label": b["label"], "flash": flash, "ram": ram, "flash_total": region, "ram_total": mem["ram_total"]})
            out["builds"] = builds
            out["builds_source"] = board["memory_source"]
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for sub in ("recipes", "boards"):
        if (OUT / sub).exists():
            shutil.rmtree(OUT / sub)
        (OUT / sub).mkdir()
    for src, dst in (("fox-red-labs-horizontal-red.svg", "logo-red.svg"), ("fox-red-labs-horizontal-cream.svg", "logo-cream.svg"),
                     ("fox-red-labs-mark-red.svg", "logo-mark.svg")):
        shutil.copy2(ROOT / "brand" / "kit" / "svg" / src, OUT / dst)
    shutil.copy2(ROOT / "tools" / "live" / "live_panel.js", OUT / "live-panel.js")   # the shared live panel (one source, tools/live/)
    shutil.copy2(ROOT / "tools" / "live" / "xlsx.js", OUT / "xlsx.js")                 # the dependency-free workbook writer
    boards, recipes = [], []
    b, r = l1306()
    boards.append(b); recipes += r
    # Discover every board definition; adding a draft must not require another hard-coded list.
    for bid in sorted(p.stem for p in (OUT / "boards-draft").glob("*.json")):
        if (OUT / "boards-draft" / f"{bid}.json").exists():
            b, r = m33(bid) if bid in M33 else draft(bid)
            boards.append(b); recipes += r
    b, r = wireless()
    boards += b; recipes += r
    from radio_recipes import entries as radio_entries      # the radio boards' demos (cc2755p10/radio-demos/build.sh)
    recipes += radio_entries(OUT)
    for b in boards:
        b["resources"] = resources(b["id"])
    for rec in recipes:
        mp = OUT / rec["manifest"] if rec.get("manifest") else ROOT / "prebuilt" / rec["board"] / rec["id"] / "manifest.json"
        man = json.loads(mp.read_text()) if mp.exists() else None
        rec["further"] = further(rec, man)
        if man and man.get("params"):
            rec["params"] = man["params"]          # the shared parameter block (tools/webflash/params.py): the Change tab reads it
        if man and (man.get("digit") or man.get("occ")):
            rec["ai"] = ai_recipes.ai_block(rec["board"], man)
        if rec["further"]["route"] != "guided":
            rec["terminal"] = rec["further"]["command"]
    note = "Written by tools/webflash/build_page.py from data/routes.json, prebuilt/, site/data/projects.json and boards-draft/. Do not edit by hand."
    common = {"usb": {"vendorId": 0x0451, "productId": 0xBEF3, "usagePage": 0xFF00}}
    # the open aperture (2026-10-17): kits TI's files describe that no run has touched, read-only, after the tested entries
    import sdk_catalog
    predicted = sdk_catalog.write(boards)
    (OUT / "catalog.json").write_text(json.dumps({"boards": catalog(ROOT, boards, recipes)}, indent=1) + "\n")   # inventory only; the page adds predicted from their file
    (OUT / "boards.json").write_text(json.dumps({"note": note, **common, "boards": boards}, indent=1, ensure_ascii=False) + "\n")
    (OUT / "recipes.json").write_text(json.dumps({"note": note, "recipes": recipes}, indent=1, ensure_ascii=False) + "\n")
    if (OUT / "board.json").exists():
        (OUT / "board.json").unlink()
    print(f"wrote {OUT.relative_to(ROOT)}/boards.json ({len(boards)} boards) and recipes.json ({len(recipes)} recipes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
