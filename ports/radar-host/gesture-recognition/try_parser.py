#!/usr/bin/env python3
"""Feed vilari-mickopf/mmwave-gesture-recognition's own config loader and frame parser (unedited) with an
IWRL6432BOOST cfg and stream, and with the repo's own AWR1642 profile.
  try_parser.py <clone dir> <our.cfg> <stream.bin>"""
import sys, traceback
import importlib.util, types

def load(name, rel):
    """Load one file of the repo as module `name`, skipping the package __init__ files (they import
    TensorFlow, scikit-learn and Qt, none of which the loader or parser use)."""
    for pkg in ("mmwave_gesture", "mmwave_gesture.data", "mmwave_gesture.communication", "mmwave_gesture.utils"):
        sys.modules.setdefault(pkg, types.ModuleType(pkg))
    spec = importlib.util.spec_from_file_location(name, f"{sys.argv[1]}/{rel}")
    mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod; spec.loader.exec_module(mod)
    return mod

load("mmwave_gesture.utils.thread_safe_print", "mmwave_gesture/utils/thread_safe_print.py")
Formats = load("mmwave_gesture.data.formats", "mmwave_gesture/data/formats.py").Formats
Parser = load("mmwave_gesture.communication.parser", "mmwave_gesture/communication/parser.py").Parser

def frames(data, magic=bytes([2, 1, 4, 3, 6, 5, 8, 7])):
    starts = [k for k in range(len(data)) if data.startswith(magic, k)] + [len(data)]
    return [data[a:b] for a, b in zip(starts, starts[1:])]

for name, cfg in [("our cfg", sys.argv[2]), ("repo profile.cfg", sys.argv[1] + "/mmwave_gesture/communication/profiles/profile.cfg")]:
    print(f"== Formats({name})")
    try:
        f = Formats(cfg); print(f"  OK: max_velocity {f.max_velocity:.3f} m/s, range bins {f.range_bins}")
    except Exception as e:
        print(f"  {type(e).__name__}: {e}"); continue
    p = Parser(f); ok = pts = bad = 0
    for fr in frames(open(sys.argv[3], "rb").read()):
        try:
            out = p.parse(bytes(fr), warn=False)
        except Exception as e:
            bad += 1; last = f"{type(e).__name__}: {e}"; continue
        if out and out["tlvs"].get("detectedPoints"):
            ok += 1; pts += len(out["tlvs"]["detectedPoints"]["objs"])
    print(f"  frames {len(frames(open(sys.argv[3], 'rb').read()))}, parsed with points {ok}, points {pts}, exceptions {bad}"
          + (f" (last: {last})" if bad else ""))
