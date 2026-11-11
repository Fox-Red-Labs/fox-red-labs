"""The Digital Lab's "Pick an AI model and see how it does": what the picker knows about each model, per board.

build_page.py calls model_recipe() for a digit image of any model and occupancy() for a room-occupancy image, and
ai_block() adds to each of those recipes an `ai` entry: the model's facts and OUR measured rows for that board
(data/results/<board>.csv, data/ai-digit-sizes.csv), each with the row it came from, so the page shows what the chip
did beside what we measured earlier. Nothing here is typed in twice: a number that has no row is left out and the page
says so. The page shows the board in hand only; one link goes to edge-ai.html for the cross-chip numbers.
"""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# What the model is, from BitNetMCU (third-party/bitnetmcu/SOURCE.md, ports/bitnetmcu/README.md) and the lab's occupancy
# sweep (mspm0g3507/mcu-bench/edgeai/README.md). Accuracy is the author's or the sweep's, on its own test set, not
# on the 10 or 20 windows the chip holds.
DIGIT = {
    "cnn_64": {"name": "64-wide CNN", "kind": "digit", "size": "11.0 kB of weights", "accuracy": "99.55 % on the MNIST test set (the model author's figure)",
               "variants": {"cycles": ["cnn64-ticlang-o3-32mhz", "cnn64-ticlang-o3-80mhz"], "size": ["cnn64-ticlang-o3-32mhz"]}},
    "cnn_16small": {"name": "16-wide CNN, small", "kind": "digit", "size": "3.2 kB of weights", "accuracy": "98.92 % on the MNIST test set (the model author's figure)",
                    "variants": {"cycles": ["cnn16-small-ticlang-o3-32mhz"], "size": ["cnn16-small-ticlang-o3-32mhz"]}},
    "12k": {"name": "12k fully connected", "kind": "digit", "size": "12.3 kB of weights", "accuracy": "99.02 % on the MNIST test set (the model author's figure)",
            "variants": {"cycles": ["12k-ticlang-o3-32mhz"], "size": ["12k-ticlang-o3-32mhz"]}},
    "1k": {"name": "1k fully connected", "kind": "digit", "size": "1.1 kB of weights", "accuracy": "94.22 % on the MNIST test set (the model author's figure)",
           "variants": {"cycles": ["1k-ticlang-o3-32mhz"], "size": ["1k-ticlang-o3-32mhz"]}},
}
OCC = {
    "occ_cnn_500": {"name": "Room occupancy, smallest", "kind": "occupancy", "size": "636 weights", "accuracy": "88.4 % of 1,001 held-out windows (the lab's sweep; 100 % empty versus occupied)"},
    "occ_cnn_2k": {"name": "Room occupancy, small", "kind": "occupancy", "size": "1,748 weights", "accuracy": "88.5 % of 1,001 held-out windows (the lab's sweep; 99.8 % empty versus occupied)"},
    "occ_cnn_8k": {"name": "Room occupancy, medium", "kind": "occupancy", "size": "8,484 weights", "accuracy": "90.9 % of 1,001 held-out windows (the lab's sweep; 100 % empty versus occupied)"},
    "occ_cnn_30k": {"name": "Room occupancy, large", "kind": "occupancy", "size": "32,836 weights", "accuracy": "85.4 % of 1,001 held-out windows (the lab's sweep; 98.6 % empty versus occupied)"},
}


def rows(board):
    p = ROOT / "data" / "results" / f"{board}.csv"
    return list(csv.DictReader(p.open())) if p.exists() else []


def _row(rs, test, variant):
    for r in rs:
        if r["test"] == test and r["variant"] == variant and r["status"] == "measured":
            return r
    return None


def _num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def measured(board, kind, model):
    """OUR measured rows for this board and model: {cycles|ms, flash, ram, right, fit, ...}, each with `row` (test:variant) and `source`."""
    rs, out = rows(board), {}
    if kind == "digit":
        f = DIGIT[model]["variants"]
        for var in f["cycles"]:
            r = _row(rs, "ai_mnist_cycles", var)
            if r:
                mhz = int(float(r["clock_mhz"]))
                out.setdefault("cycles", []).append({"value": int(float(r["value"])), "clockMhz": mhz, "ms": round(float(r["value"]) / mhz / 1000, 1),
                                                     "row": f"ai_mnist_cycles:{var}", "source": r["source"], "note": r["note"], "date": r["date"]})
        sizes = [x for x in csv.DictReader((ROOT / "data" / "ai-digit-sizes.csv").open()) if x["board"] == board and x["variant"] in f["size"]]
        if sizes:
            s = sizes[0]
            out["size"] = {"flash": int(s["image_flash_b"]), "ram": int(s["image_ram_b"]), "stack": int(s["stack_b"]) if s["stack_b"] else None, "clockMhz": int(s["clock_mhz"]),
                           "row": f"ai-digit-sizes:{s['variant']}", "source": s["source"], "note": s["note"]}
    else:
        r = _row(rs, "ai_occ_latency", f"{model}-c-o3")
        if r:
            out["ms"] = {"value": float(r["value"]), "worst": _num(r["value_hi"]), "clockMhz": int(float(r["clock_mhz"])), "row": f"ai_occ_latency:{model}-c-o3", "source": r["source"], "date": r["date"],
                         "note": "the fastest of the 20 windows, best of 5, the model's run call with the float input quantised"}
        r = _row(rs, "ai_occ_fit", model)
        if r:
            out["fit"] = {"row": f"ai_occ_fit:{model}", "note": r["conditions"], "source": r["source"]}
        else:
            r = next((x for x in rs if x["test"] == "ai_occ_fit" and x["variant"] == model), None)
            if r:
                out["fit"] = {"row": f"ai_occ_fit:{model}", "status": r["status"], "note": r["conditions"], "source": r["source"]}
        fl, rm = _row(rs, "ai_model_flash", f"{model}-c-o3"), _row(rs, "ai_model_ram", f"{model}-c-o3")
        if fl and rm:
            out["size"] = {"flash": int(float(fl["value"])), "ram": int(float(rm["value"])), "row": f"ai_model_flash:{model}-c-o3", "source": fl["source"],
                           "note": "the model's own flash (weights and code) and RAM, harness and stack not counted"}
    return out


def ai_block(board, m):
    """The `ai` entry of a recipe, from its manifest m."""
    if m.get("digit"):
        model, kind = m["digit"]["model"], "digit"
        facts = DIGIT[model]
    else:
        model, kind = m["occ"]["model"], "occupancy"
        facts = OCC[model]
    return {"family": kind, "model": model, "name": facts["name"], "size": facts["size"], "accuracy": facts["accuracy"],
            "image": m.get("memory"), "measured": measured(board, kind, model)}


def digit_recipe(m, did, kit, PROMPT, CLONE):
    model = m["digit"]["model"]
    f = DIGIT[model]
    d = m["digit"]
    title = "Draw a digit: the chip reads it" if model == "cnn_64" else f"Draw a digit: {f['name']} reads it"
    return {
        "id": did, "recipe": "digit", "project": "handwritten-digits", "title": title,
        "short": "Draw a digit (AI)" if model == "cnn_64" else f"Digit: {f['name']}",
        "outcome": f"An edge AI model (BitNetMCU's {model}, {f['size']}) runs on the chip. You draw a digit here; the page sends it through the USB cable and the chip answers with the digit and how long it took.",
        "see": "Draw a digit from 0 to 9 with your mouse or finger. The chip's answer and its time appear beside it.",
        "before": "", "wiring": None,
        "check": {"kind": "digit", **{k: d[k] for k in ("addr", "magic")}, "model": model},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"BitNetMCU (Tim Schneider, GPL-3.0) {model} model and inference code, unchanged, in the Fox Red Labs mailbox program "
                  f"({m['source']['file']}) on TI's gpio_toggle_output example for the {kit}, MSPM0 SDK {m['tools']['mspm0_sdk']['version']}, "
                  f"TI Arm Clang {m['tools']['ticlang']['version']}",
        "terminal": CLONE + "./start.sh " + m["board"] + " blink",
        "prompt": PROMPT.format(kit=kit, want="I want to run an edge AI model that reads handwritten digits."),
    }


def occupancy(m, did, kit, PROMPT, CLONE):
    model = m["occ"]["model"]
    f = OCC[model]
    o = m["occ"]
    return {
        "id": did, "recipe": "occupancy", "project": "small-ai-model", "title": f"Count the people in a room: {f['name'].split(', ')[-1]} model",
        "short": f"Room: {f['name'].split(', ')[-1]} model",
        "outcome": f"An edge AI model ({model}, {f['size']}) reads a recorded 32-sample window of light, sound, CO2 and motion and says how many people were in the room, 0 to 3. The chip holds 20 windows from data it was not trained on; pick one and the chip answers.",
        "see": "Pick a window. The chip's count appears beside the true count, with the time it took.",
        "before": "", "wiring": None,
        "check": {"kind": "occ", "addr": o["addr"], "magic": o["magic"], "model": model, "labels": o["labels"]},
        "sha256": m["image"]["sha256"], "bytes": m["image"]["segments"][0]["bytes"],
        "source": f"The lab's {model} room-occupancy model (UCI Room Occupancy Estimation data, CC BY 4.0), plain C, in the Fox Red Labs mailbox program "
                  f"({m['source']['file']}) on TI's gpio_toggle_output example for the {kit}, MSPM0 SDK {m['tools']['mspm0_sdk']['version']}, TI Arm Clang {m['tools']['ticlang']['version']}",
        "terminal": CLONE + "./start.sh " + m["board"] + " blink",
        "prompt": PROMPT.format(kit=kit, want="I want to run an edge AI model that counts the people in a room from sensor readings."),
    }
