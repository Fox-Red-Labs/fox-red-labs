"""One hash over a list of repository files, the way the validation records write it: sorted by path, each path then its bytes.
tools/code_size_check.py and tools/edge_ai_check.py write it into their records as engine_sha."""
import hashlib, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent


def sha(paths):
    h = hashlib.sha256()
    for p in sorted(paths):
        h.update(p.encode()); h.update((ROOT / p).read_bytes())
    return h.hexdigest()[:16]
