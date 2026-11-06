#!/usr/bin/env python3
"""Build the browser bench's extra starter examples for MSPM0 LaunchPad kits (Digital Lab, site-feedback F1).

  python3 tools/webflash/build_examples.py [board ...] [--only sos,fast,button,breathe]

Default board: mspm0g3519 (the board each example was verified on; the LP-MSPM0C1104 builds too, but its debugger could not reach the chip on 2026-10-19). Same route as build_board.py:
TI's gpio_toggle_output example for the board is copied from the validated SDK into a scratch folder (deleted
afterwards), its main source is replaced by a file in tools/webflash/player/, board_leds.h is written from
boards_mspm0.py, and the example's own makefile builds it with the validated tools. Writes prebuilt/<board>/<id>/
with a manifest. The page data comes from `python3 tools/webflash/build_page.py`.

  sos      blink.c with Morse S O S              blink-sos
  fast     blink.c, 80 ms on and 80 ms off       blink-fast
  button   button.c, S2 lights the red LED       button-led
  breathe  breathe.c, software PWM breathing     breathe
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_board  # noqa: E402
import params  # noqa: E402
from boards_mspm0 import BOARDS  # noqa: E402

ROOT = build_board.ROOT
PLAYER = build_board.PLAYER
# Morse S O S: dot 150 ms, dash 450 ms, 150 ms between marks, 450 ms between letters, 1050 ms between words.
SOS = [150, 150, 150, 150, 150, 450, 450, 150, 450, 150, 450, 450, 150, 150, 150, 150, 150, 1050]
FAST = [80, 80]


def pins(b):
    return {k: ({"label": c[3], "bit": c[1], "gpio": f"{c[2]:#010x}".replace("0X", "0x")} if c else None) for k, c in b["channels"].items()}


def mailbox(tmp, symbol):
    """Address of a global in the linker map (the image is stripped by then)."""
    text = (tmp / "p" / "ticlang" / "gpio_toggle_output.map").read_text()
    m = re.search(rf"^([0-9a-f]{{8}})\s+{symbol}\s*$", text, re.M)
    if not m:
        raise SystemExit(f"{symbol} not in the map")
    return int(m.group(1), 16)


def blink(board, key, pattern, title, short, see, outcome):
    b = BOARDS[board]
    src = PLAYER / "blink.c"
    img, t, notice, tmp = build_board.build(board, src, ", ".join(map(str, pattern)), params_kind="blink", pattern=pattern)
    try:
        led = pins(b)["red"]
        build_board.write(board, f"blink-{key}", img, t, notice, tmp, "blink-pattern",
                          {"pattern_ms": pattern, "led": led, "active_high": b["active_high"], "params": params.manifest_block("blink", b["table"], pattern),
                           "title": title.format(led=led["label"].split(" (")[0]), "short": short, "see": see.format(led=led["label"]), "outcome": outcome,
                           "seconds": round(sum(pattern) / 1000 * 1.8 + 2, 1)},
                          src, "PATTERN_MS = " + ", ".join(map(str, pattern)), f"python3 tools/webflash/build_examples.py {board} --only {key}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def button(board):
    b = BOARDS[board]
    src = PLAYER / "button.c"
    img, t, notice, tmp = build_board.build(board, src)
    try:
        port, bit, gpio, iomux, label = b["button"]
        led = pins(b)["red"]
        build_board.write(board, "button-led", img, t, notice, tmp, "button",
                          {"led": led, "active_high": b["active_high"],
                           "button": {"label": label, "bit": bit, "gpio": f"{gpio:#010x}".replace("0X", "0x"), "pressed_level": 0, "pull": "internal pull-up"},
                           "expect": f"{led['label']} is lit while {label} is held and dark when it is let go"},
                          src, "the example's main source replaced by the button-to-LED program", f"python3 tools/webflash/build_examples.py {board} --only button")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def breathe(board):
    b = BOARDS[board]
    src = PLAYER / "breathe.c"
    img, t, notice, tmp = build_board.build(board, src, params_kind="breathe")
    try:
        addr = mailbox(tmp, "br")
        led = pins(b)["red"]
        build_board.write(board, "breathe", img, t, notice, tmp, "breathe",
                          {"led": led, "active_high": b["active_high"], "params": params.manifest_block("breathe", b["table"]),
                           "breathe": {"addr": f"0x{addr:08X}", "magic": "0xB2EA7401", "layout": ["magic", "duty", "breaths"], "period_ms": 2000,
                                       "pwm_hz": 1000, "step_percent": 1},
                           "expect": f"{led['label']} slowly brightens and dims, about every two seconds"},
                          src, "the example's main source replaced by the breathing-LED program", f"python3 tools/webflash/build_examples.py {board} --only breathe")
        print(f"   mailbox br at 0x{addr:08X}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


EXAMPLES = {
    "sos": lambda bd: blink(bd, "sos", SOS, "Blink {led}: Morse S O S", "Blink: SOS",
                            "{led} sends three short flashes, three long, three short, then a pause.",
                            "The Morse call for help, from TI's LED example with a longer pattern. The page times the LED through the board's debugger to check it."),
    "fast": lambda bd: blink(bd, "fast", FAST, "Blink {led}: fast, 80 ms on and 80 ms off", "Blink: fast",
                             "{led} flickers about six times a second.",
                             "The same program as the plain blink at a fast rate. The page times the LED through the board's debugger to check it."),
    "button": button,
    "breathe": breathe,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("boards", nargs="*", default=["mspm0g3519"])
    ap.add_argument("--only", default=",".join(EXAMPLES))
    a = ap.parse_args()
    for bd in a.boards:
        for k in a.only.split(","):
            if k == "button" and not BOARDS[bd].get("button"):
                print(f"skip {bd}/{k}: no button recorded")
                continue
            EXAMPLES[k](bd)


if __name__ == "__main__":
    main()
