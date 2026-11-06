#!/usr/bin/env python3
"""Prebuilt firmware with a release manifest, and the checks that make flashing it safe.

  python3 tools/frl/prebuilt.py make --board mspm0l1306 --pattern 150,150,150,900 --out prebuilt/mspm0l1306/blink-150-150-150-900
  python3 tools/frl/prebuilt.py make --board mspm0l1306 --recipe print-message --message "Hello from my LaunchPad" --out prebuilt/mspm0l1306/message-hello
  python3 tools/frl/frl.py flash-prebuilt <folder>/manifest.json --probe S      (in frl.py)

A prebuilt image lets someone see the known demo before installing a compiler. The manifest ties the image
to its board (by chip ID and kit), its exact bytes (sha256), the source it came from (repo commit and the
edited source's sha256), the tools that built it, the memory it writes, and the result it should show.
Flashing refuses a different board, an altered image, or an image that writes outside the route's limits,
then runs the same check as a source build. Rebuild from source with `frl run`; the manifest says how.

Redistribution: the program is TI's BSD-3-Clause example plus TI DriverLib, linked with TI Arm Clang's
runtime library (its SPDX file lists permissive licenses, including BSD-4-Clause parts). NOTICE.txt carries
TI's notice. Whether that is enough for a public release is not settled here: "release_cleared" stays false
until someone checks the runtime's notices.
"""
import argparse
import datetime as dt
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ops  # noqa: E402
import target  # noqa: E402
from core import ROOT, Result, Stop, recipe, stop  # noqa: E402


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def make(board, ask, out, project=None, recipe_name="blink-pattern"):
    b, r = recipe(board, recipe_name)
    res = ops.prepare(board, recipe_name, ask, project)
    if not res["ok"]:
        raise Stop(res)
    built = ops.build(board, recipe_name, res["data"]["project"])
    if not built["ok"]:
        raise Stop(built)
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    img = out / "firmware.out"
    shutil.copy2(built["data"]["image"], img)
    # debug sections carry the build machine's folder names; the released image keeps only what loads
    import toolfind
    strip = Path(toolfind.find("ticlang", r["tools"]["compiler"].get("validated"))["path"]) / "bin" / "tiarmstrip"
    subprocess.run([str(strip), "--strip-all", str(img)], check=True)
    src = Path(res["data"]["source"])
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    man = {
        "format": "frl-prebuilt-1",
        "board": board, "kit": b["kit"], "revisions_tested": ["unknown: the board revision was not recorded for this build"],
        "chip": {"deviceid": b["target"]["identity"]["deviceid"], "mask": b["target"]["identity"]["deviceid_mask"]},
        "recipe": recipe_name,
        **({"message": res["data"]["text"]} if r.get("edit") == "message" else {"pattern_ms": res["data"]["pattern_ms"]}),
        "image": {"file": img.name, "sha256": sha(img), "bytes": img.stat().st_size, "segments": target.check_writes(b, img)},
        "source": {"repo_commit": commit, "example": r["example"], "file": r["source_file"], "sha256": sha(src),
                   "edited_line": res["data"]["edited_line"],
                   "rebuild": (f"python3 tools/frl/frl.py run --board {board} --recipe {recipe_name} --message \"{res['data']['text']}\" --probe <serial>"
                               if r.get("edit") == "message" else
                               f"python3 tools/frl/frl.py run --board {board} --pattern {','.join(map(str, res['data']['pattern_ms']))} --probe <serial>")},
        "tools": built["data"]["tools"],
        "expect": (f"the board prints \"> {res['data']['text']} N\" once a second on its USB serial port at {b['console']['baud']} baud, N counting; frl verify checks it"
                   if r.get("edit") == "message" else
                   f"{r['led']} repeats on/off {','.join(map(str, res['data']['pattern_ms']))} ms; frl verify checks it"),
        "made": dt.datetime.now().isoformat(timespec="seconds"),
        "release_cleared": False,
        "release_note": "TI example (BSD-3-Clause, NOTICE.txt) linked with TI Arm Clang's runtime (permissive licenses per its SPDX file, not reviewed line by line). Not cleared for public distribution.",
    }
    (out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n", encoding="utf-8")
    head = src.read_text(encoding="utf-8").split("*/", 1)[0] + "*/\n"
    what = "with its message changed" if r.get("edit") == "message" else "with its LED timing changed"
    (out / "NOTICE.txt").write_text(f"This firmware is built from Texas Instruments' MSPM0 SDK example {Path(r['example']).name}, "
                                    f"{what}. TI's notice from that source:\n\n" + head, encoding="utf-8")
    return Result.good(f"prebuilt image and manifest in {out}", manifest=str(out / "manifest.json"), sha256=man["image"]["sha256"])


def load(manifest):
    """The manifest and its image, checked: format, board route, sha256 and write limits."""
    m = json.loads(Path(manifest).read_text(encoding="utf-8"))
    if m.get("format") != "frl-prebuilt-1":
        stop("refused", "FRL-E031", f"{manifest} is not a prebuilt manifest this tool knows")
    b, r = recipe(m["board"], m["recipe"])
    img = Path(manifest).parent / m["image"]["file"]
    if not img.is_file() or sha(img) != m["image"]["sha256"]:
        stop("refused", "FRL-E031", f"{img.name} is missing or is not the image the manifest names (sha256 differs)",
             "download the release again, or build from source with the manifest's rebuild line")
    if (b["target"]["identity"]["deviceid"], b["target"]["identity"]["deviceid_mask"]) != (m["chip"]["deviceid"], m["chip"]["mask"]):
        stop("refused", "FRL-E005", f"the manifest is for chip {m['chip']['deviceid']}, this route expects {b['target']['identity']['deviceid']}")
    target.check_writes(b, img)
    return m, img


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("make")
    p.add_argument("--board", required=True)
    p.add_argument("--recipe", default="blink-pattern")
    p.add_argument("--pattern")
    p.add_argument("--message")
    p.add_argument("--out", required=True)
    p.add_argument("--project")
    a = ap.parse_args()
    try:
        res = make(a.board, a.message if a.recipe == "print-message" else a.pattern, a.out, a.project, a.recipe)
    except Stop as s:
        res = s.result
    print(res.text())
    return res.exit_status()


if __name__ == "__main__":
    sys.exit(main())
