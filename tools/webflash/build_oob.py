#!/usr/bin/env python3
"""Build TI's out-of-box demo for a LaunchPad kit from the MSPM0 SDK, unchanged, as a prebuilt image.

  python3 tools/webflash/build_oob.py mspm0l1306        writes prebuilt/mspm0l1306/out-of-box/

The bench page puts it on a board that no longer has it and then runs TI's demo GUI's role itself: it talks
to the demo over the board's USB serial port in MessagePack (the demo's callbacks: blinkEnable, blinkTick,
lightEnable, thermEnable; readings lsADC and thADC), so nothing needs installing and no GUI access request.
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
import target  # noqa: E402
import toolfind  # noqa: E402
from core import recipe  # noqa: E402

KITS = {"mspm0l1306": "LP_MSPM0L1306"}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    board = sys.argv[1] if len(sys.argv) > 1 else "mspm0l1306"
    b, r = recipe(board, "blink-pattern")
    v = {k: r["tools"][k].get("validated") for k in ("sdk", "sysconfig", "compiler")}
    sdk = toolfind.find("mspm0_sdk", v["sdk"])
    syscfg = toolfind.find("sysconfig", v["sysconfig"])
    cc = toolfind.find("ticlang", v["compiler"])
    make = toolfind.find("make")
    example = f"examples/nortos/{KITS[board]}/demos/out_of_box"
    with tempfile.TemporaryDirectory(prefix="frl-oob-") as tmp:
        proj = Path(tmp) / "out_of_box"
        shutil.copytree(Path(sdk["path"]) / example, proj)
        p = subprocess.run([make["exe"], "-C", str(proj / "ticlang"), f"MSPM0_SDK_INSTALL_DIR={sdk['path']}",
                            f"SYSCONFIG_TOOL={syscfg['path']}", f"TICLANG_ARMCOMPILER={cc['path']}", "GCC_ARMCOMPILER="],
                           capture_output=True, text=True, timeout=600)
        built = proj / "ticlang" / "out_of_box.out"
        if p.returncode or not built.is_file():
            print((p.stdout + p.stderr)[-1500:])
            return 1
        out = ROOT / "prebuilt" / board / "out-of-box"
        out.mkdir(parents=True, exist_ok=True)
        img = out / "firmware.out"
        shutil.copy2(built, img)
        notice = (proj / "main.c").read_text().split("*/", 1)[0] + "*/\n"
    subprocess.run([str(Path(cc["path"]) / "bin" / "tiarmstrip"), "--strip-all", str(img)], check=True)
    segs = target.check_writes(b, img)
    man = {
        "format": "frl-prebuilt-1", "board": board, "kit": b["kit"],
        "revisions_tested": ["unknown: the board revision was not recorded for this build"],
        "chip": {"deviceid": b["target"]["identity"]["deviceid"], "mask": b["target"]["identity"]["deviceid_mask"]},
        "recipe": "out-of-box",
        "oob": {"baud": b["console"]["baud"], "commands": {"blink": "blinkEnable", "blink_delay": "blinkTick", "light": "lightEnable",
                                                            "thermometer": "thermEnable"},
                "readings": {"light": "lsADC", "temperature_c": "thADC"}},
        "image": {"file": img.name, "sha256": sha(img), "bytes": img.stat().st_size, "segments": segs},
        "source": {"repo_commit": subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip(),
                   "example": example, "file": "main.c", "sha256": sha(Path(sdk["path"]) / example / "main.c"),
                   "edited_line": "none: TI's demo as it is in the SDK", "rebuild": f"python3 tools/webflash/build_oob.py {board}"},
        "tools": {"sdk": {"version": sdk["version"]}, "sysconfig": {"version": syscfg["version"]}, "compiler": {"version": cc["version"]}},
        "expect": "TI's out-of-box demo: LED1 blinks; over the USB serial port it answers MessagePack commands and sends light and temperature readings",
        "made": dt.datetime.now().isoformat(timespec="seconds"), "release_cleared": False,
        "release_note": "TI's out-of-box demo from the MSPM0 SDK (BSD-3-Clause, NOTICE.txt), unchanged, with TI Arm Clang's runtime. Not cleared for public distribution.",
    }
    (out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
    (out / "NOTICE.txt").write_text(f"Texas Instruments' out-of-box demo for the {b['kit']} from the MSPM0 SDK {sdk['version']} ({example}), "
                                    "unchanged. TI's notice from its source:\n\n" + notice)
    print(f"OK: {out.relative_to(ROOT)}: {segs[0]['bytes']} bytes, sha256 {man['image']['sha256'][:16]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
