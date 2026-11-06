"""One folder per run in the per-user data folder: an append-only journal and the current state.

States move forward: created, identified, ready, backed_up or backup_unavailable, built, programming,
programmed, then verified, unverified or failed; cancelled and refused can happen at any point. While
the state is 'programming' a write may be in flight: a run found in that state was interrupted, and the
board is unknown until checked. Times are split three ways: machine (build, flash, check), wait
(downloads and installs) and hands_on (a person's time). Hands-on time this tool cannot see is
recorded as null, meaning unknown, never zero.
"""
import datetime as dt
import json
import os
import secrets

from core import data_dir, host, VERSION

STATES = ["created", "identified", "ready", "backed_up", "backup_unavailable", "built", "programming",
          "programmed", "verified", "unverified", "failed", "cancelled", "refused", "done"]
FINAL = {"verified", "unverified", "failed", "cancelled", "refused", "done"}


def runs_dir():
    d = data_dir() / "runs"
    d.mkdir(parents=True, exist_ok=True)
    return d


def now():
    return dt.datetime.now().isoformat(timespec="seconds")


class Run:
    def __init__(self, run_id):
        self.id = run_id
        self.dir = runs_dir() / run_id
        self.state_file = self.dir / "state.json"

    @classmethod
    def new(cls, **fields):
        rid = dt.datetime.now().strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(3)
        r = cls(rid)
        r.dir.mkdir(mode=0o700)
        st = {"run": rid, "interface": VERSION, "state": "created", "made": now(), "host": host(),
              "timing": {"machine_s": {}, "wait_s": {}, "hands_on_s": None}, "steps": [], **fields}
        r._write(st)
        r.log("created", fields)
        return r

    @classmethod
    def load(cls, run_id):
        r = cls(run_id)
        if not r.state_file.is_file():
            raise FileNotFoundError(run_id)
        return r

    def state(self):
        return json.loads(self.state_file.read_text(encoding="utf-8"))

    def _write(self, st):
        tmp = self.state_file.with_suffix(".tmp")
        tmp.write_text(json.dumps(st, indent=1) + "\n", encoding="utf-8")
        os.replace(tmp, self.state_file)

    def log(self, event, data=None):
        with open(self.dir / "journal.jsonl", "a", encoding="utf-8") as f:
            f.write(json.dumps({"t": now(), "event": event, "data": data or {}}) + "\n")

    def set(self, state=None, step=None, **fields):
        st = self.state()
        if state:
            assert state in STATES, state
            st["state"] = state
            st["updated"] = now()
        if step:
            st["steps"].append({"t": now(), **step})
        st.update(fields)
        self._write(st)
        self.log(state or "update", {**(step or {}), **{k: v for k, v in fields.items() if k != "timing"}})
        return st

    def time(self, bucket, name, seconds):
        st = self.state()
        st["timing"][bucket][name] = round(st["timing"][bucket].get(name, 0) + seconds, 2)
        self._write(st)

    def summary(self):
        """A short handoff a fresh person or assistant can act on, without the conversation that made it."""
        st = self.state()
        last = st["steps"][-1] if st["steps"] else {}
        what = st.get("recipe") or st.get("op") or "?"
        lines = [f"Run {st['run']}: {st.get('board') or '?'} / {what}, probe {st.get('probe') or 'not selected'}, "
                 f"on {st['host']['node']} ({st['host']['os']} {st['host']['machine']})",
                 f"State: {st['state']}. Last step: {last.get('name', 'none')} -> {last.get('code', '')} {last.get('message', '')}".rstrip(),
                 f"Project folder: {st.get('project') or 'not made yet'}",
                 f"Backup: {st.get('backup') or 'none'}"]
        if st["state"] == "programming":
            lines.append(f"A write may have been interrupted. Check the board first: python3 tools/frl/frl.py verify-image --board {st.get('board')} "
                         f"--image {st.get('image') or (st.get('writing') or {}).get('image') or '<image>'} --probe {st.get('probe')}")
        elif st["state"] not in FINAL:
            lines.append(f"Continue: frl run --resume {st['run']}")
        if last.get("next"):
            lines.append(f"Next: {last['next']}")
        return "\n".join(lines)


def latest(board=None):
    for d in sorted(runs_dir().iterdir(), reverse=True):
        try:
            st = json.loads((d / "state.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if not board or st.get("board") == board:
            return Run(d.name)
    return None


def redact(text, serials=()):
    """A copy safe to paste into a public report: no home folder, user name, computer name or full probe serial."""
    import getpass
    import platform as _p
    import re as _re
    from pathlib import Path as _P
    out = text.replace(str(_P.home()), "~")
    for word in {getpass.getuser(), _p.node(), _p.node().split(".")[0]}:
        if word and len(word) > 2:
            out = out.replace(word, "<redacted>")
    for s_ in serials:
        if s_ and len(s_) > 4:
            out = out.replace(s_, "<probe ..." + s_[-2:] + ">")
    out = _re.sub(r"\b(\d{1,3}\.){3}\d{1,3}\b", "<ip>", out)
    return out
