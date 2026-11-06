"""Shared pieces of the local setup tool: paths, the result type, stable error codes, route data.

Standard library only, Python 3.9 or later. Every operation returns a Result with a stable code, one
human sentence, a next action and structured data. A person, the CLI, an assistant and the MCP adapter
all read the same Result; none of them may turn a failure into a success.
"""
import json
import os
import platform
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROUTES = ROOT / "data" / "routes.json"
PROJECTS = ROOT / "site" / "data" / "projects.json"
CODE = ROOT / "site" / "data" / "code.json"
VERSION = "0.1.0"          # the operation interface; adapters check it (docs/frl.md, "Interface version")

# Stable codes. Each has a FAQ entry in docs/frl-errors.md under the same heading, so the visible code
# is searchable. Never reuse or renumber a code; retire it instead.
ERRORS = {
    "FRL-OK": "done",
    "FRL-E001": "no debug probe found on USB",
    "FRL-E002": "more than one debug probe and none selected",
    "FRL-E003": "the selected probe is not on USB",
    "FRL-E004": "the probe is in use by another run",
    "FRL-E005": "the board behind the probe is not the board you named",
    "FRL-E006": "could not read the board through the probe",
    "FRL-E007": "no connect step by that name",
    "FRL-E010": "this board is not in the supported routes",
    "FRL-E011": "this project does not run on this board",
    "FRL-E012": "this project has not been tried on this board",
    "FRL-E020": "a required tool is missing",
    "FRL-E021": "only an unvalidated version of a tool is installed",
    "FRL-E022": "several validated versions of a tool are installed",
    "FRL-E023": "the assistant command is not installed",
    "FRL-E030": "the build failed",
    "FRL-E031": "the image to flash is missing or changed",
    "FRL-E032": "programming failed",
    "FRL-E033": "programming was interrupted; the board state is unknown",
    "FRL-E034": "the write would touch memory this route may not change",
    "FRL-E035": "this route does not write the board yet: its flash driver would erase more than main flash",
    "FRL-E036": "the image needs a different flash layout from the one on the board",
    "FRL-E040": "the check failed: the board did not do what was asked",
    "FRL-E041": "not verified: no check exists for this result",
    "FRL-E042": "not verified: the check could not read the board",
    "FRL-E050": "no backup was made",
    "FRL-E051": "the backup does not belong to this board",
    "FRL-E052": "the backup files changed since they were saved",
    "FRL-E053": "the backup names no image to put back",
    "FRL-E060": "the assistant stopped with an error",
    "FRL-E061": "the assistant ran past its time limit",
    "FRL-E070": "bad input",
    "FRL-E099": "unexpected error",
}
# Exit status by outcome: 0 verified or done, 1 failed, 2 refused before any target access, 3 not verified.
EXIT = {"ok": 0, "failed": 1, "refused": 2, "unverified": 3}


class Result(dict):
    """{"ok", "outcome", "code", "message", "next", "data"}; outcome is ok, failed, refused or unverified."""

    @classmethod
    def make(cls, outcome, code, message="", nxt="", **data):
        assert code in ERRORS, code
        return cls(ok=outcome == "ok", outcome=outcome, code=code, message=message or ERRORS[code],
                   next=nxt, faq=f"docs/frl-errors.md#{code.lower()}" if code != "FRL-OK" else "", data=data)

    @classmethod
    def good(cls, message="done", **data):
        return cls.make("ok", "FRL-OK", message, **data)

    def exit_status(self):
        return EXIT[self["outcome"]]

    def text(self):
        head = "OK" if self["ok"] else f"{self['code']} {self['outcome'].upper()}"
        out = f"{head}: {self['message']}"
        if self.get("next"):
            out += f"\n  next: {self['next']}"
        if self.get("faq"):
            out += f"\n  help: {self['faq']}"
        return out


class Stop(Exception):
    """Raised inside an operation to return a Result early."""

    def __init__(self, result):
        super().__init__(result["message"])
        self.result = result


def stop(outcome, code, message="", nxt="", **data):
    raise Stop(Result.make(outcome, code, message, nxt, **data))


def host():
    """Where this process runs. A run is bound to the computer that has the board, not the one showing chat."""
    return {"node": platform.node(), "os": platform.system(), "release": platform.release(),
            "machine": platform.machine(), "python": platform.python_version(),
            "virtualized": _virtualized()}


def _virtualized():
    """Best guess whether this is a VM or container (a Cowork or Codex guest sees files, maybe not USB)."""
    if Path("/.dockerenv").exists() or Path("/run/.containerenv").exists():
        return "container"
    try:
        if "hypervisor" in Path("/proc/cpuinfo").read_text(encoding="utf-8"):
            return "vm"
    except OSError:
        pass
    if platform.system() == "Darwin":
        try:
            import subprocess
            r = subprocess.run(["sysctl", "-n", "kern.hv_vmm_present"], capture_output=True, text=True, timeout=5)
            if r.stdout.strip() == "1":
                return "vm"
        except Exception:
            pass
    return "no"


def data_dir():
    """Durable per-user state outside any clone or worktree: runs, backups, locks. FRL_HOME overrides."""
    if os.environ.get("FRL_HOME"):
        d = Path(os.environ["FRL_HOME"])
    elif sys.platform == "darwin":
        d = Path.home() / "Library" / "Application Support" / "FoxRedLabs"
    elif os.name == "nt":
        d = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local")) / "FoxRedLabs"
    else:
        d = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")) / "fox-red-labs"
    d.mkdir(parents=True, exist_ok=True)
    try:
        os.chmod(d, 0o700)
    except OSError:
        pass
    return d


def routes():
    return json.loads(ROUTES.read_text(encoding="utf-8"))


def board_route(board):
    b = routes()["boards"].get(board)
    if not b:
        known = ", ".join(sorted(routes()["boards"]))
        stop("refused", "FRL-E010", f"{board!r} has no supported route yet (supported: {known})",
             "run `frl identify <name printed on the board>` for its official resources and an honest manual path")
    return b


def recipe(board, name):
    b = board_route(board)
    r = b["recipes"].get(name)
    if not r:
        stop("refused", "FRL-E011", f"{b['kit']} has no {name!r} route (it has: {', '.join(b['recipes'])})",
             "pick one of the listed routes")
    return b, r


def hexint(s):
    return int(s, 16) if isinstance(s, str) else int(s)
