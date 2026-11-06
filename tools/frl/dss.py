"""The "dss" transport: frl's identity, write, LED sample, backup and restore for a C2000 through CCStudio's
scripting (tools/frl/dss/frl_c28.js), because OpenOCD has no C28x support. target.py and backup.py call these
when a route says "transport": "dss". Each call opens one debug session; addresses are C28x word addresses.
"""
import re
import subprocess
import tempfile
from pathlib import Path

import toolfind
from core import hexint, stop

HERE = Path(__file__).resolve().parent


def ccxml(board, serial):
    """The route's connection file with this probe's serial in both serial fields."""
    d = Path(tempfile.mkdtemp(prefix="frl-dss-"))
    p = d / "target.ccxml"
    p.write_text((HERE / "ccxml" / board["target"]["ccxml"]).read_text(encoding="utf-8").replace("{SERIAL}", serial))
    return p


def run(board, serial, args, timeout=600):
    """(exit, output) of one frl_c28.js operation."""
    rs = toolfind.find("ccs_scripting")
    try:
        import os
        env = dict(os.environ, FRL_C28_ENTRY=board["target"].get("entry", "0x80000"))   # where a flash boot starts
        p = subprocess.run([rs["exe"], str(HERE / "dss" / "frl_c28.js"), str(ccxml(board, serial)), *map(str, args)],
                           capture_output=True, text=True, timeout=timeout, env=env)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or "") if isinstance(e.stdout, str) else ""


def _failed(out, serial, what):
    msg = next((ln.strip() for ln in (out or "").splitlines() if ln.startswith("fatal:") or ln.startswith("Error:")), "no answer")
    stop("failed", "FRL-E006", f"CCStudio could not {what} the board on probe {serial}: {msg[:200]}",
         "check the board's power and USB cable; close CCStudio or UniFlash if they hold the probe; then try again", dss=(out or "")[-600:])


def identity(board, serial):
    code, out = run(board, serial, ["identity"], timeout=180)
    m = re.search(r"^ID (\d+) (\d+)$", out or "", re.M)
    if code != 0 or not m:
        _failed(out, serial, "read")
    return {"deviceid": f"0x{int(m.group(1)):08X}", "partidl": f"0x{int(m.group(2)):08X}"}


def program(board, serial, image):
    code, out = run(board, serial, ["program", image])
    if code is None:
        stop("failed", "FRL-E033", "CCStudio stopped answering during programming; the board state is unknown",
             "do not write again blindly: run `frl restore` from the backup", dss=(out or "")[-600:])
    if code != 0 or "OK programmed" not in out:
        _failed(out, serial, "program")
    return out


def verify_image(board, serial, image):
    """(matches, output): CCStudio compares main flash with the image; read-only."""
    code, out = run(board, serial, ["verify", image])
    if code != 0 or not re.search(r"^(OK verified|MISMATCH)", out or "", re.M):
        _failed(out, serial, "read")
    return "OK verified" in out, out


def sample(board, serial, led, seconds):
    """Same shape as target.sample: transitions, samples, span, output_enabled. Active-low LEDs are inverted, so 1 is lit."""
    code, out = run(board, serial, ["sample", led["gpio_dat"], led["gpio_dir"], led["bit"], seconds], timeout=180 + seconds)
    if code != 0 or not re.search(r"^N \d+ \d+$", out or "", re.M):
        _failed(out, serial, "sample")
    inv = 1 if led.get("active_low") else 0
    trans = [(int(t), int(b) ^ inv) for t, b in re.findall(r"^T (\d+) (\d)$", out, re.M)]
    n, span = map(int, re.search(r"^N (\d+) (\d+)$", out, re.M).groups())
    return {"transitions": trans, "samples": n, "span_ms": span, "output_enabled": int(re.search(r"^E (\d)$", out, re.M).group(1))}


def backup_region(board, serial, addr, words, out):
    code, o = run(board, serial, ["backup", f"0x{addr:X}", words, out])
    if code != 0 or "OK saved" not in o:
        _failed(o, serial, "read")


def restore_region(board, serial, image, addr, words, back):
    code, o = run(board, serial, ["restore", image, f"0x{addr:X}", words, back])
    if code != 0 or "OK restored" not in o:
        _failed(o, serial, "restore")


def check_identity(board, serial):
    ident = identity(board, serial)
    i = board["target"]["identity"]
    mask = hexint(i.get("deviceid_mask", "0xFFFFFFFF"))
    accepted = [i["deviceid"]] + list(i.get("deviceid_any") or [])
    if not any(hexint(ident["deviceid"]) & mask == hexint(x) & mask for x in accepted):
        stop("refused", "FRL-E005", f"probe {serial} is attached to a C2000 with PARTIDH {ident['deviceid']}, not the {board['kit']} ({', '.join(accepted)})",
             f"check the name printed on the board ({board['printed']}); pick the matching board, or the right --probe", identity=ident)
    return ident
