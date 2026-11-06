#!/usr/bin/env python3
"""Print this bench's own probe serial for a board, from the git-ignored tools/benchmark/settings.local.json
("lab_probes": {"<board>": "<serial>"}), or nothing. Bench scripts use it in place of a hard-coded serial,
so a copied command on anyone else's machine has no default and asks for PROBE instead.

  python3 tools/frl/labprobe.py mspm0l1306
"""
import json
import sys
from pathlib import Path

try:
    s = json.loads((Path(__file__).resolve().parents[1] / "benchmark" / "settings.local.json").read_text(encoding="utf-8"))
    print((s.get("lab_probes") or {}).get(sys.argv[1], ""))
except (OSError, ValueError, IndexError):
    pass
