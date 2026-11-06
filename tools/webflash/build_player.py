#!/usr/bin/env python3
"""Build the LP-MSPM0L1306 color player and write it as a prebuilt image with a manifest.

  python3 tools/webflash/build_player.py        writes prebuilt/mspm0l1306/color-player/

TI's gpio_toggle_output example is copied from the validated SDK into a scratch project (its SysConfig file sets up
LED2's three pins), its main source is replaced by mspm0l1306/color-player/color_player.c, and it is built with
frl's own build step and validated tools. The manifest has the same shape as tools/frl/prebuilt.py's, plus the
table the browser bench writes ("table").
"""
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
import ops  # noqa: E402
import target  # noqa: E402
import toolfind  # noqa: E402
from core import recipe  # noqa: E402

SRC = ROOT / "mspm0l1306" / "color-player" / "color_player.c"
OUT = ROOT / "prebuilt" / "mspm0l1306" / "color-player"
TABLE = {"addr": "0x0000FC00", "magic": "0x52434C46", "max_steps": 32, "min_ms": 20, "max_ms": 10000,
         "bits": {"red": 0, "green": 1, "blue": 2},
         "pins": {"red": {"label": "LED2 red (PA26)", "bit": 26}, "green": {"label": "LED2 green (PA13)", "bit": 13},
                  "blue": {"label": "LED2 blue (PA27)", "bit": 27}},
         "default": [[1, 300], [2, 300], [4, 300], [0, 300]]}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    b, r = recipe("mspm0l1306", "blink-pattern")
    sdk = toolfind.find("mspm0_sdk", r["tools"]["sdk"].get("validated"))
    with tempfile.TemporaryDirectory(prefix="frl-color-player-") as tmp:
        proj = Path(tmp) / "color-player"
        shutil.copytree(Path(sdk["path"]) / r["example"], proj)
        for junk in ("iar", "keil", "gcc"):
            shutil.rmtree(proj / junk, ignore_errors=True)
        notice = (proj / r["source_file"]).read_text().split("*/", 1)[0] + "*/\n"
        shutil.copy2(SRC, proj / r["source_file"])           # the makefile builds this file name
        built = ops.build("mspm0l1306", "blink-pattern", str(proj))
        if not built["ok"]:
            print(built.text())
            return 1
        OUT.mkdir(parents=True, exist_ok=True)
        img = OUT / "firmware.out"
        shutil.copy2(built["data"]["image"], img)
    strip = Path(toolfind.find("ticlang", r["tools"]["compiler"].get("validated"))["path"]) / "bin" / "tiarmstrip"
    subprocess.run([str(strip), "--strip-all", str(img)], check=True)
    segs = target.check_writes(b, img)
    end = max(int(s["addr"], 16) + s["bytes"] for s in segs)
    if end > int(TABLE["addr"], 16):
        print(f"the image reaches 0x{end:X}, into the table sector at {TABLE['addr']}")
        return 1
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    man = {
        "format": "frl-prebuilt-1",
        "board": "mspm0l1306", "kit": b["kit"], "revisions_tested": ["unknown: the board revision was not recorded for this build"],
        "chip": {"deviceid": b["target"]["identity"]["deviceid"], "mask": b["target"]["identity"]["deviceid_mask"]},
        "recipe": "color-player", "table": TABLE,
        "image": {"file": img.name, "sha256": sha(img), "bytes": img.stat().st_size, "segments": segs},
        "source": {"repo_commit": commit, "example": r["example"], "file": str(SRC.relative_to(ROOT)), "sha256": sha(SRC),
                   "edited_line": "the example's main source replaced by the color player",
                   "rebuild": "python3 tools/webflash/build_player.py"},
        "tools": built["data"]["tools"],
        "expect": "LED2 plays the color sequence in the table at 0x0000FC00 (red, green, blue, off at 300 ms each when there is none)",
        "made": dt.datetime.now().isoformat(timespec="seconds"),
        "release_cleared": False,
        "release_note": "Built on TI's example (BSD-3-Clause, NOTICE.txt) with TI DriverLib, linked with TI Arm Clang's runtime (permissive licenses per its SPDX file, not reviewed line by line). Not cleared for public distribution.",
    }
    (OUT / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
    (OUT / "NOTICE.txt").write_text("This firmware is the Fox Red Labs color player, built on Texas Instruments' MSPM0 SDK example "
                                    "gpio_toggle_output (its SysConfig setup and DriverLib). TI's notice from that example:\n\n" + notice)
    print(f"OK: color player in {OUT.relative_to(ROOT)}: {segs[0]['bytes']} bytes at {segs[0]['addr']}, sha256 {man['image']['sha256'][:16]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
