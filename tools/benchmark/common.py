"""Shared helpers for the benchmark tools: repo paths, the local settings file, TI tool discovery on
macOS, Linux and Windows, and a small client for CCStudio's MCP servers.

Standard library only. Paths come from the repo root or from tools/benchmark/settings.local.json
(git-ignored, never shared). Windows paths are written but not tested on a Windows machine yet.
"""
import csv
import glob
import json
import os
import platform
import queue
import re
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data"
HERE = Path(__file__).resolve().parent
SETTINGS = HERE / "settings.local.json"
LOCAL = HERE / "local"  # backups, bundles, scratch: git-ignored

# Board folders for boards whose folder name is not the board id. Arrivals carry their own "folder".
FOLDERS = {
    "mspm0g3507": "mspm0g3507",
    "cc2340r5": "cc2340r5",
    "f2800137": "c2000-launchpad",
    "cc3200": "cc3200-launchxl",
}


def system():
    s = platform.system()
    return {"Darwin": "mac", "Windows": "windows"}.get(s, "linux")


def exe(path):
    """Add .exe on Windows."""
    return str(path) + (".exe" if system() == "windows" and not str(path).endswith(".exe") else "")


def load_settings():
    try:
        return json.loads(SETTINGS.read_text())
    except (OSError, json.JSONDecodeError):
        return {}


def save_settings(s):
    SETTINGS.write_text(json.dumps(s, indent=1) + "\n")


def ti_roots():
    """Folders TI installers use by default, plus any the user set in the settings file."""
    s = load_settings()
    roots = [s["ti_root"]] if s.get("ti_root") else []
    home = Path.home()
    roots += {
        "mac": ["/Applications/ti", str(home / "ti")],
        "linux": [str(home / "ti"), "/opt/ti"],
        "windows": ["C:\\ti", str(home / "ti")],
    }[system()]
    seen, out = set(), []
    for r in roots:
        if r not in seen and Path(r).is_dir():
            seen.add(r)
            out.append(Path(r))
    return out


def find_ccs():
    """Newest CCStudio install: the folder that holds ccs_base. None if missing."""
    s = load_settings()
    if s.get("ccs"):
        p = Path(s["ccs"])
        return p if (p / "ccs_base").is_dir() else None
    found = []
    for r in ti_roots():
        for d in glob.glob(str(r / "ccs*" / "ccs")):
            if (Path(d) / "ccs_base").is_dir():
                found.append(Path(d))
    return sorted(found)[-1] if found else None


def ccs_version(ccs):
    """'2101' style folder name to '21.0.1'."""
    name = ccs.parent.name.replace("ccs", "")
    return f"{name[:2]}.{name[2]}.{name[3:]}" if name.isdigit() and len(name) >= 3 else name


def find_mcp_proxy(ccs):
    """CCStudio's MCP server launcher (mcp-server-proxy.js) and its own Node.js, or (None, None)."""
    if not ccs:
        return None, None
    node = Path(exe(ccs / "tools" / "node" / "node"))
    pats = ["*/Contents/Resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js",  # macOS app bundle
            "*/resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js",  # Linux, Windows (unverified)
            "*/*/resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js",
            "*/*/*/resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js"]
    for p in pats:
        hits = glob.glob(str(ccs / p))
        if hits:
            return Path(hits[0]), (node if node.exists() else None)
    return None, (node if node.exists() else None)


def find_xdsdfu(ccs):
    if not ccs:
        return None
    p = Path(exe(ccs / "ccs_base" / "common" / "uscif" / "xds110" / "xdsdfu"))
    return p if p.exists() else None


def find_dslite(ccs):
    """DSLite from CCStudio first, then from UniFlash."""
    cands = []
    if ccs:
        cands.append(Path(exe(ccs / "ccs_base" / "DebugServer" / "bin" / "DSLite")))
    for r in ti_roots():
        for u in sorted(glob.glob(str(r / "uniflash_*")), reverse=True):
            sub = {"mac": "osx", "linux": "linux", "windows": "win"}[system()]
            cands.append(Path(exe(Path(u) / "deskdb" / "content" / "TICloudAgent" / sub / "ccs_base" / "DebugServer" / "bin" / "DSLite")))
    for c in cands:
        if c.exists():
            return c
    return None


def find_targetdb(ccs):
    p = ccs / "ccs_base" / "common" / "targetdb" if ccs else None
    return p if p and p.is_dir() else None


SDK_PREFIXES = {
    "mspm0_sdk_": "MSPM0 SDK",
    "mspm33_sdk_": "MSPM33 SDK",
    "am13e230x_sdk_": "AM13E2x SDK",
    "simplelink_lowpower_f3_sdk_": "SimpleLink Low Power F3 SDK",
    "simplelink_cc13xx_cc26xx_sdk_": "SimpleLink CC13xx CC26xx SDK",
    "simplelink_wifi_sdk_": "SimpleLink Wi-Fi SDK",
    "C2000Ware_": "C2000Ware",
    "c2000ware_core_": "C2000Ware core",
    "uniflash_": "UniFlash",
    "sysconfig_": "SysConfig (standalone)",
}


def find_sdks():
    """{product name: [(version folder, path)]} from the TI install folders (one level, plus c2000/)."""
    out = {}
    for r in ti_roots():
        for d in list(r.iterdir()) + (list((r / "c2000").iterdir()) if (r / "c2000").is_dir() else []):
            for pre, name in SDK_PREFIXES.items():
                if d.is_dir() and d.name.startswith(pre):
                    out.setdefault(name, []).append((d.name[len(pre):], d))
    return out


def load_boards():
    with (DATA / "boards.csv").open() as f:
        return {r["board"]: r for r in csv.DictReader(f)}


def load_suite():
    return json.loads((DATA / "benchmark-suite.json").read_text())


def load_arrivals():
    return json.loads((DATA / "arrivals.json").read_text())


def board_folder(board):
    for it in load_arrivals().get("items", []):
        if it.get("board_id") == board and it.get("folder"):
            return it["folder"].rstrip("/")
    if board in FOLDERS:
        return FOLDERS[board]
    return board


def load_rows(board):
    p = DATA / "results" / f"{board}.csv"
    if not p.exists():
        return []
    with p.open() as f:
        return list(csv.DictReader(f))


class Mcp:
    """One CCStudio MCP server (project, debug, sysconfig or serial) over stdio, through CCStudio's own
    proxy. CCStudio must be open with an AI-enabled workspace. Works the same on every OS: a reader
    thread instead of select(), which Windows pipes do not support."""

    def __init__(self, server, ccs=None, timeout=20):
        ccs = ccs or find_ccs()
        proxy, node = find_mcp_proxy(ccs)
        if not proxy or not node:
            raise RuntimeError("CCStudio MCP proxy not found (is CCStudio 21 or newer installed?)")
        env = dict(os.environ)
        env.pop("ELECTRON_RUN_AS_NODE", None)
        ws = load_settings().get("ccs_workspace") or str(Path.home() / "workspace_ccstheia")
        self.p = subprocess.Popen([str(node), str(proxy), server], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, env=env,
                                  cwd=ws if Path(ws).is_dir() else None)
        self.q = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()
        self.n = 0
        self._call("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                  "clientInfo": {"name": "benchmark", "version": "1"}}, timeout)
        self._send({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _read(self):
        for line in self.p.stdout:
            try:
                self.q.put(json.loads(line))
            except json.JSONDecodeError:
                pass
        self.q.put(None)

    def _send(self, o):
        self.p.stdin.write(json.dumps(o) + "\n")
        self.p.stdin.flush()

    def _call(self, method, params, timeout):
        self.n += 1
        self._send({"jsonrpc": "2.0", "id": self.n, "method": method, "params": params})
        end = time.time() + timeout
        while time.time() < end:
            try:
                d = self.q.get(timeout=max(0.1, end - time.time()))
            except queue.Empty:
                break
            if d is None:
                raise RuntimeError("CCStudio MCP server closed the connection")
            if d.get("id") == self.n:
                if "error" in d:
                    raise RuntimeError(str(d["error"]))
                return d.get("result")
        raise TimeoutError(f"{method}: no answer in {timeout} s")

    def tool(self, name, args=None, timeout=60):
        r = self._call("tools/call", {"name": name, "arguments": args or {}}, timeout)
        text = "".join(c.get("text", "") for c in (r or {}).get("content", []))
        if (r or {}).get("isError"):
            raise RuntimeError(text or "tool error")
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text

    def close(self):
        try:
            self.p.kill()
        except Exception:
            pass


def ccs_running():
    """True when a CCStudio process is running (MCP servers answer only then)."""
    try:
        if system() == "windows":
            out = subprocess.run(["tasklist"], capture_output=True, text=True, timeout=10).stdout
        else:
            out = subprocess.run(["ps", "-A", "-o", "command"], capture_output=True, text=True, timeout=10).stdout
    except Exception:
        return False
    return "Code Composer Studio" in out or "ccstudio" in out.lower()


def disk_free_gb(path=ROOT):
    import shutil
    return shutil.disk_usage(path).free / 1024 ** 3


# What each board line needs, where to get it, and how big it is. Sizes measured on macOS
# (mac-setup/README.md, disk table); other systems differ a little.
DOWNLOADS = {
    "CCStudio": ("https://www.ti.com/tool/download/CCSTUDIO", "1.9 GB download, 8.1 GB installed, about 10 GB free while installing"),
    "MSPM0 SDK": ("https://www.ti.com/tool/download/MSPM0-SDK", "0.4 GB download, 2.2 GB installed"),
    "MSPM33 SDK": ("https://www.ti.com/tool/download/MSPM33-SDK", "0.27 GB download, 0.8 GB installed"),
    "AM13E2x SDK": ("https://www.ti.com/tool/download/AM13E2X-SDK", "0.04 GB download, 0.2 GB installed"),
    "C2000Ware": ("https://www.ti.com/tool/C2000WARE", "0.3 GB download, 1.6 GB installed"),
    "SimpleLink Low Power F3 SDK": ("https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK", "0.55 GB download, 2.0 GB installed plus 2.2 GB of extras"),
    "SimpleLink CC13xx CC26xx SDK": ("https://www.ti.com/tool/download/SIMPLELINK-CC13XX-CC26XX-SDK", "size not measured here"),
    "SimpleLink Wi-Fi SDK": ("https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK", "0.7 GB download, 0.7 GB installed plus 1.0 GB of extras"),
    "UniFlash": ("https://www.ti.com/tool/download/UNIFLASH", "0.3 GB download, 1.9 GB installed"),
}

SDK_FOR_BOARD = {
    "cc2340r5": "SimpleLink Low Power F3 SDK",
    "cc2755p10": "SimpleLink Low Power F3 SDK",
    "cc1354p10": "SimpleLink CC13xx CC26xx SDK",
    "cc35x1": "SimpleLink Wi-Fi SDK",
}
SDK_FOR_LINE = {"MSPM0": "MSPM0 SDK", "MSPM33": "MSPM33 SDK", "AM13": "AM13E2x SDK", "C2000": "C2000Ware"}


def sdk_for(board, boards=None):
    """The TI SDK a board's tests build with, or None for other vendors."""
    boards = boards or load_boards()
    b = boards.get(board, {})
    if b.get("vendor") != "TI":
        return None
    return SDK_FOR_BOARD.get(board) or SDK_FOR_LINE.get(b.get("line"))


def ti_public_page(board_id, board_name):
    """Is there a public TI.com page for this part? Tries the board's tool page, then the chip's product
    page (board id in capitals). Returns (True, url), (False, urls tried) when every page answers 404, or
    (None, reason) when TI.com could not be checked. Plain HTTP GET, nothing sent but the URL."""
    import urllib.error
    import urllib.request
    urls = [f"https://www.ti.com/tool/{board_name}", f"https://www.ti.com/product/{board_id.upper()}"]
    missing = []
    for u in urls:
        try:
            r = urllib.request.urlopen(urllib.request.Request(u, headers={"User-Agent": "benchmark-check/1"}), timeout=15)
            if r.status == 200:
                return True, u
        except urllib.error.HTTPError as e:
            if e.code == 404:
                missing.append(u)
                continue
            return None, f"{u} answered HTTP {e.code}"
        except Exception as e:
            return None, f"{u}: {type(e).__name__}: {e}"
    return False, " and ".join(missing)


SAFE_ID = re.compile(r"[a-z0-9][a-z0-9_-]{0,63}")       # board ids
SAFE_SERIAL = re.compile(r"[A-Za-z0-9]{1,32}")           # probe serial numbers
SAFE_DEVICE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,63}")  # targetdb device names


def need(rx, value, what):
    """Exit unless value is a plain name: values from the command line or a file become file names."""
    if not value or not rx.fullmatch(value) or value in (".", ".."):
        raise SystemExit(f"{what} {value!r} is not a plain name")
    return value
