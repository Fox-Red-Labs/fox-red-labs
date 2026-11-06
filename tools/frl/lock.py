#!/usr/bin/env python3
"""One owner per physical probe, across processes, assistants, worktrees and clones.

The lock lives in the per-user data folder (never in a clone or worktree, so two worktrees still share
it) and is an OS file lock: it is released when its process dies, so a crashed run never leaves the
probe blocked. The owner record beside it says who holds it and what they were doing. When a new run
takes the lock and finds a record the last owner never cleared while it was writing, the board state is
unknown: the caller must check the board before writing again.

  python3 tools/frl/lock.py exec --serial S -- <command ...>   run one command while holding S's lock
  python3 tools/frl/lock.py status [--serial S]                  who holds what

Children inherit FRL_LOCK_HELD=<serial> so a script called by the lock holder (ocd.sh, blink.py) does
not wait on its own parent. This is cooperation, not enforcement: CCStudio or another debugger that does
not use this lock can still hold the probe, and the tools report that as a busy probe.
"""
import argparse
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from core import data_dir, host, stop  # noqa: E402

if os.name == "nt":
    import msvcrt
else:
    import fcntl


def _dir():
    d = data_dir() / "locks"
    d.mkdir(parents=True, exist_ok=True)
    return d


class ProbeLock:
    """with ProbeLock(serial, run_id, op) as lk: ...   lk.previous is the stale record, if any."""

    def __init__(self, serial, run_id="", op=""):
        self.serial, self.run_id, self.op = serial, run_id, op
        self.path = _dir() / f"{serial}.lock"
        self.owner = _dir() / f"{serial}.owner.json"
        self.fh = None
        self.previous = None
        self.nested = os.environ.get("FRL_LOCK_HELD") == serial

    def __enter__(self):
        if self.nested:
            return self
        self.fh = open(self.path, "a+", encoding="utf-8")
        try:
            if os.name == "nt":
                msvcrt.locking(self.fh.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                fcntl.flock(self.fh.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.fh.close()
            self.fh = None
            who = read_owner(self.serial) or {}
            stop("refused", "FRL-E004",
                 f"probe {self.serial} is in use by {who.get('op') or 'another run'} (pid {who.get('pid', '?')}, run {who.get('run_id') or '?'}, since {who.get('since', '?')})",
                 "wait for it to finish, or stop that run; this tool never kills another process", owner=who)
        prev = read_owner(self.serial)
        if prev:   # the last holder died without clearing its record
            self.previous = prev
        self.write_owner("holding")
        os.environ["FRL_LOCK_HELD"] = self.serial
        return self

    def write_owner(self, state):
        if self.nested:
            return
        rec = {"serial": self.serial, "pid": os.getpid(), "host": host()["node"], "run_id": self.run_id,
               "op": self.op, "state": state, "since": dt.datetime.now().isoformat(timespec="seconds")}
        tmp = self.owner.with_suffix(".tmp")
        tmp.write_text(json.dumps(rec) + "\n", encoding="utf-8")
        os.replace(tmp, self.owner)

    def __exit__(self, *exc):
        if self.nested or not self.fh:
            return False
        if self.owner.exists():
            self.owner.unlink()
        os.environ.pop("FRL_LOCK_HELD", None)
        if os.name == "nt":
            try:
                self.fh.seek(0)
                msvcrt.locking(self.fh.fileno(), msvcrt.LK_UNLCK, 1)
            except OSError:
                pass
        else:
            fcntl.flock(self.fh.fileno(), fcntl.LOCK_UN)
        self.fh.close()
        return False


def read_owner(serial):
    try:
        return json.loads((_dir() / f"{serial}.owner.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("exec")
    e.add_argument("--serial", required=True)
    e.add_argument("--op", default="command")
    e.add_argument("argv", nargs=argparse.REMAINDER)
    s = sub.add_parser("status")
    s.add_argument("--serial")
    a = ap.parse_args()
    if a.cmd == "status":
        for f in sorted(_dir().glob("*.owner.json")):
            if not a.serial or f.name.startswith(a.serial + "."):
                print(f.read_text(encoding="utf-8").strip())
        return 0
    argv = a.argv[1:] if a.argv[:1] == ["--"] else a.argv
    from core import Stop
    try:
        with ProbeLock(a.serial, op=a.op) as lk:
            if lk.previous and lk.previous.get("state") == "writing":
                print(f"warning: the last run on {a.serial} stopped while writing ({lk.previous}); check the board before trusting it", file=sys.stderr)
            return subprocess.call(argv)
    except Stop as s:
        print(f"{s.result['code']}: {s.result['message']}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    sys.exit(main())
