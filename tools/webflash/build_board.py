#!/usr/bin/env python3
"""Build the browser bench's color player and blink image for one MSPM0 LaunchPad (not the L1306: build_player.py).

  python3 tools/webflash/build_board.py mspm0c1104 [--pattern 150,150,150,900]

TI's gpio_toggle_output example for the board is copied from the validated SDK into a scratch folder (deleted
afterwards), its main source is replaced by tools/webflash/player/{color_player,blink}.c, board_leds.h is written
from tools/webflash/boards_mspm0.py, and the example's own makefile builds it with the validated tools. Writes
prebuilt/<board>/color-player/ and prebuilt/<board>/blink-<pattern>/ with manifests shaped like the L1306's.
"""
import argparse
import datetime as dt
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "frl"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import params  # noqa: E402
import toolfind  # noqa: E402
from boards_mspm0 import BOARDS, SDK  # noqa: E402

VALID = {"mspm0_sdk": ["2.11.00.07"], "sysconfig": ["1.26.3", "1.28.0"], "ticlang": ["5.1.1.LTS"], "make": None}
PLAYER = ROOT / "tools" / "webflash" / "player"


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def header(b):
    ch = b["channels"]
    lines = [f"/* Written by tools/webflash/build_board.py for the {b['kit']}. */",
             f"#define PL_TABLE_ADDR ({b['table']:#010x}u)", f"#define PL_ACTIVE_HIGH {1 if b['active_high'] else 0}"]
    for k, name in (("red", "R"), ("green", "G"), ("blue", "B")):
        c = ch[k]
        lines.append(f"#define PL_{name}_PORT ({c[0]})" if c else f"#define PL_{name}_PORT (GPIOA)")
        lines.append(f"#define PL_{name}_PIN (DL_GPIO_PIN_{c[1]})" if c else f"#define PL_{name}_PIN (0u)")
    if b.get("button"):
        port, bit, _, iomux, _ = b["button"]
        lines += [f"#define PL_SW_PORT ({port})", f"#define PL_SW_PIN (DL_GPIO_PIN_{bit})", f"#define PL_SW_IOMUX ({iomux})"]
    return "\n".join(lines) + "\n"


def build(board, src, extra="", params_kind=None, pattern=None):
    """Returns (image path, tools dict, segments, notice). Caller removes the scratch folder.
    params_kind: the image's kind in params.py ("blink", "breathe", ...): frl_params.h and frl_params_gen.h go in the project."""
    b = BOARDS[board]
    t = {k: toolfind.find(k, v) for k, v in VALID.items()}
    tmp = Path(tempfile.mkdtemp(prefix=f"frl-{board}-"))
    proj = tmp / "p"
    shutil.copytree(Path(t["mspm0_sdk"]["path"]) / b["example"], proj)
    for junk in ("iar", "keil", "gcc"):
        shutil.rmtree(proj / junk, ignore_errors=True)
    notice = (proj / "gpio_toggle_output.c").read_text().split("*/", 1)[0] + "*/\n"
    text = src.read_text().replace("PATTERN_MS", extra) if extra else src.read_text()
    (proj / "gpio_toggle_output.c").write_text(text)
    (proj / "board_leds.h").write_text(header(b))
    shutil.copy2(PLAYER / "frl_params.h", proj / "frl_params.h")
    (proj / "frl_params_gen.h").write_text(params.gen_header(params_kind, pattern))
    args = [t["make"]["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={t['mspm0_sdk']['path']}",
            f"SYSCONFIG_TOOL={t['sysconfig']['path']}", f"TICLANG_ARMCOMPILER={t['ticlang']['path']}", "GCC_ARMCOMPILER="]
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    img = proj / "ticlang" / "gpio_toggle_output.out"
    if p.returncode or not img.is_file():
        print((p.stdout + p.stderr)[-2000:])
        raise SystemExit(f"build failed ({board})")
    strip = Path(t["ticlang"]["path"]) / "bin" / "tiarmstrip"
    subprocess.run([str(strip), "--strip-all", str(img)], check=True)
    return img, t, notice, tmp


def map_memory(map_text):
    """Used flash and RAM of an image, bytes, from the linker map's memory table (whole image: harness, driver library, stack)."""
    import re
    rows = {}
    for m in re.finditer(r"^\s+(FLASH|SRAM\w*)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})", map_text, re.M):
        rows[m.group(1)] = rows.get(m.group(1), 0) + int(m.group(4), 16)
    return {"flash_b": rows.get("FLASH", 0), "ram_b": sum(v for k, v in rows.items() if k.startswith("SRAM")),
            "from": "the linker map of this image: bytes used in FLASH and SRAM, test program, driver library, model and stack included"}


def segments(img):
    """Loadable segments of a 32-bit ELF: [{addr, bytes}] (the same shape as the L1306 manifests)."""
    d = img.read_bytes()
    phoff = int.from_bytes(d[28:32], "little"); ent = int.from_bytes(d[42:44], "little"); num = int.from_bytes(d[44:46], "little")
    out = []
    for k in range(num):
        o = phoff + k * ent
        typ, _, _, paddr, filesz = (int.from_bytes(d[o + i:o + i + 4], "little") for i in (0, 4, 8, 12, 16))
        if typ == 1 and filesz:
            out.append({"addr": f"0x{paddr:08X}", "bytes": filesz})
    return out


def write(board, dirname, img, t, notice, tmp, recipe, extra_man, src, edited, rebuild):
    b = BOARDS[board]
    out = ROOT / "prebuilt" / board / dirname
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(img, out / "firmware.out")
    segs = segments(out / "firmware.out")
    end = max(int(s["addr"], 16) + s["bytes"] for s in segs)
    limit = b["table"] if recipe == "color-player" or "params" in extra_man else b["flash_end"]   # the last sector holds the page's settings
    if end > limit:
        raise SystemExit(f"the image reaches {end:#x}, past {limit:#x}")
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    man = {
        "format": "frl-prebuilt-1", "board": board, "kit": b["kit"],
        "revisions_tested": ["unknown: the board revision was not recorded for this build"],
        "chip": {"deviceid": f"{b['deviceid']:#010X}".replace("0X", "0x"), "mask": "0x0FFFFFFF"},
        "recipe": recipe, **extra_man,
        "image": {"file": "firmware.out", "sha256": sha(out / "firmware.out"), "bytes": (out / "firmware.out").stat().st_size, "segments": segs},
        "source": {"repo_commit": commit, "example": b["example"], "file": str(src.relative_to(ROOT)), "sha256": sha(src),
                   "edited_line": edited, "rebuild": rebuild},
        "tools": {k: {"version": v["version"], "validated": v["validated"], "source": v["source"]} for k, v in t.items()},
        "made": dt.datetime.now().isoformat(timespec="seconds"), "release_cleared": False,
        "release_note": "Built on TI's example (BSD-3-Clause, NOTICE.txt) with TI DriverLib, linked with TI Arm Clang's runtime. Not cleared for public distribution.",
    }
    (out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
    (out / "NOTICE.txt").write_text("This firmware is built on Texas Instruments' MSPM0 SDK example gpio_toggle_output "
                                    "(its SysConfig setup and DriverLib). TI's notice from that example:\n\n" + notice)
    print(f"OK {out.relative_to(ROOT)}: {segs}, sha256 {man['image']['sha256'][:16]}, ends {end:#x} (limit {limit:#x})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board", choices=sorted(BOARDS))
    ap.add_argument("--pattern", default="150,150,150,900")
    ap.add_argument("--only-blink", action="store_true", help="skip the color player (its image has not changed)")
    a = ap.parse_args()
    b = BOARDS[a.board]
    pins = {k: ({"label": c[3], "bit": c[1], "gpio": f"{c[2]:#010x}".replace("0X", "0x")} if c else None) for k, c in b["channels"].items()}
    # color player
    if not a.only_blink:
        src = PLAYER / "color_player.c"
        img, t, notice, tmp = build(a.board, src)
        try:
            table = {"addr": f"{b['table']:#010x}".replace("0X", "0x"), "magic": "0x52434C46", "max_steps": 32, "min_ms": 20, "max_ms": 10000,
                     "bits": {"red": 0, "green": 1, "blue": 2}, "pins": {k: v for k, v in pins.items() if v},
                     "active_high": b["active_high"], "default": [[1, 300], [2, 300], [4, 300], [0, 300]]}
            write(a.board, "color-player", img, t, notice, tmp, "color-player",
                  {"table": table, "expect": "the LED plays the color sequence in the table in the last flash sector (red, green, blue, off at 300 ms each when there is none)"},
                  src, "the example's main source replaced by the color player", f"python3 tools/webflash/build_board.py {a.board}")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    # blink
    pat = [int(x) for x in a.pattern.split(",")]
    src = PLAYER / "blink.c"
    img, t, notice, tmp = build(a.board, src, a.pattern.replace(",", ", "), params_kind="blink", pattern=pat)
    try:
        write(a.board, "blink-" + a.pattern.replace(",", "-"), img, t, notice, tmp, "blink-pattern",
              {"pattern_ms": pat, "led": pins["red"], "active_high": b["active_high"], "params": params.manifest_block("blink", b["table"], pat)},
              src, f"PATTERN_MS = {a.pattern}", f"python3 tools/webflash/build_board.py {a.board} --pattern {a.pattern}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
