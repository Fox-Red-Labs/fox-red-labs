#!/usr/bin/env python3
"""Write site/data/motorlink-vars.json, the variable map the Motor console page uses.

The indices come from the firmware's own C table (link.load_vars), never from the SDK's JSON, whose indices disagree
with it. The tables of motor states, fault bits and bench-guard words sit beside them so the page and the Python
client decode the same way.

  python3 tools/motorlink/varmap.py           write the file
  python3 tools/motorlink/varmap.py --check   exit 1 if the file differs from the firmware table
"""
import hashlib, json, pathlib, re, sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import link

ROOT = link.ROOT
OUT = ROOT / "site" / "data" / "motorlink-vars.json"

# frlGuard.state and frlGuard.reason: the numbers are read from am13e230/foc-drv8376/frl_guard.h so they cannot drift; the words are ours,
# keyed by the header's names. A name in the header with no words here stops the build.
HEADER = ROOT / "am13e230" / "foc-drv8376" / "frl_guard.h"
STATE_WORDS = {"IDLE": "idle", "CALIBRATING": "calibrating", "RUNNING": "running", "STOPPED": "stopped by the guard"}
REASON_WORDS = {"NONE": "none", "CAP": "cap reached", "HOST_LOST": "host lost", "OVERCURRENT": "over current", "TI_FAULT": "controller fault",
                "BUS": "bus out of range", "REFUSED_FAULT": "refused: controller fault present", "REFUSED_BUS": "refused: bus out of range",
                "REFUSED_UNCALIBRATED": "refused: Hall table not calibrated", "HOST_STOP": "host stop", "CAL_DONE": "calibration done",
                "BG_STALL": "background stall",
                "REFUSED_OFFSET": "refused: currents not near zero at standstill", "CLOCK": "guard clock check failed",
                "STOP_OVERCURRENT": "over current while stopping"}


def _enum(prefix, words):
    src = HEADER.read_text()
    found = {m.group(1): int(m.group(2)) for m in re.finditer(prefix + r"(\w+)\s*=\s*(\d+)", src)}
    missing = sorted(set(found) - set(words))
    if missing or not found:
        raise SystemExit(f"frl_guard.h has {prefix}{missing or 'nothing'} with no words in tools/motorlink/varmap.py")
    return {n: words[k] for k, n in found.items()}


GUARD_STATES = _enum(r"FRL_(?=(?:IDLE|CALIBRATING|RUNNING|STOPPED)\b)", STATE_WORDS)
GUARD_REASONS = _enum(r"FRL_R_", REASON_WORDS)


LAYOUTS = pathlib.Path(__file__).resolve().parent / "layouts.json"


def guard_version():
    m = re.search(r"#define FRL_GUARD_VERSION\s+(\d+)U", HEADER.read_text())
    if not m:
        raise SystemExit("FRL_GUARD_VERSION not found in frl_guard.h")
    return int(m.group(1))


def layout_sha(vs):
    """the table's layout: every index, name and type, in order"""
    return hashlib.sha256("\n".join(f"{v['index']} {v['name']} {v['type']}" for v in vs).encode()).hexdigest()[:16]


def check_layout(vs):
    """A layout change without a version bump would let a page built for one table write wrong variables on another.
    layouts.json records which table each guard version means; the same version with a different table fails."""
    ver, sha = guard_version(), layout_sha(vs)
    known = json.loads(LAYOUTS.read_text()) if LAYOUTS.exists() else {}
    if str(ver) in known and known[str(ver)] != sha:
        raise SystemExit(f"DRIFT: the MotorVision table changed (layout {sha}) but FRL_GUARD_VERSION is still {ver} "
                         f"(layout {known[str(ver)]}); bump FRL_GUARD_VERSION in frl_guard.h")
    return ver, sha, known


def build():
    vs = sorted(link.load_vars().values(), key=lambda v: v["index"])
    ver, sha, _ = check_layout(vs)
    return {
        "guard_version": ver,
        "layout_sha": sha,
        "note": "Written by tools/motorlink/varmap.py from the firmware's C table (motorvision_hash.c); never edit by hand. "
                "Permissions come from the SDK JSON by name. States and fault bits are from tools/motorlink/link.py; "
                "guard words from am13e230/foc-drv8376/frl_guard.h.",
        "vars": [{"name": v["name"], "index": v["index"], "type": v["type"], "permissions": v["permissions"]} for v in vs],
        "states": {str(k): v for k, v in link.STATES.items()},
        "fault_bits": list(link.FAULT_BITS),
        "guard_states": {str(k): v for k, v in GUARD_STATES.items()},
        "guard_reasons": {str(k): v for k, v in GUARD_REASONS.items()},
    }


def text():
    return json.dumps(build(), indent=1) + "\n"


def is_current():
    return OUT.exists() and OUT.read_text() == text()


if __name__ == "__main__":
    if "--check" in sys.argv:
        if not is_current():
            print("DRIFT: site/data/motorlink-vars.json differs from the firmware table; run python3 tools/motorlink/varmap.py"); sys.exit(1)
        print("motorlink variable map OK")
    else:
        OUT.write_text(text())
        b = build(); known = json.loads(LAYOUTS.read_text()) if LAYOUTS.exists() else {}
        known[str(b["guard_version"])] = b["layout_sha"]; LAYOUTS.write_text(json.dumps(known, indent=1) + "\n")
        print("wrote", OUT.relative_to(ROOT), len(b["vars"]), "variables, guard version", b["guard_version"])
