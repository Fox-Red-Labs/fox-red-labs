"""The "ccs13" transport: frl's identity, write, pattern sample, backup and restore for a CC13x4 or CC26x4
(LP-CC1354P10-1) through CCStudio's Python scripting (tools/frl/ccs/frl_cc13x4.py). target.py and backup.py call
these when a route says "transport": "ccs13".

Why not DSLite: on an external LP-XDS110 this chip takes a debugger only in the first seconds after power-on and
can refuse the second connect a DSLite load makes (Error -242, 14 of 14 on 2026-10-24). One CCStudio session per
step connects once and writes with loadBinaryProgram, which loaded 12 of 12 images (cc1354p10/FAQ.md).

The write covers main flash only, and only the 2 KB sectors the image's segments touch (hw_nvmnw.h,
NVMNW_GBLINFO0_SECTORSIZE_TWOKB), each block at its own address in one session; the preflight names those sectors.
The image's CCFG segment is left out, so the board keeps its own CCFG. The pattern check times the program's own
toggle log (frl_log_ms, frl_log_n) after a halt, since the GPIO output register cannot be read while the program
runs; at the halt, the LED's own output-enable bit and output bit must agree with the log (Codex finding 5).

A refused connect (Error -242) cycles the probe's USB port power when the bench names one: FRL_POWER_CYCLE=<hub>:<port>,
or "power_cycle": {"<probe serial>": "<hub>:<port>"} in tools/benchmark/settings.local.json; only when uhubctl lists
this probe on that port and both uhubctl commands succeed (power_cycle, Codex finding 7). The error number is in
CCStudio's own log line, not in the exception the script sees, so the decision is made here.
"""
import json
import os
import re
import struct
import subprocess
import tempfile
import time
from pathlib import Path

import toolfind
from core import hexint, stop

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "ccs" / "frl_cc13x4.py"
SETTINGS = HERE.parent / "benchmark" / "settings.local.json"
MAIN_END = 0x00100000            # 1 MB main flash on the CC1354P10
SECTOR = 0x800                   # its flash sector, the unit loadBinaryProgram erases


def ccxml(board, serial):
    d = Path(tempfile.mkdtemp(prefix="frl-ccs13-"))
    p = d / "target.ccxml"
    p.write_text((HERE / "ccxml" / board["target"]["ccxml"]).read_text(encoding="utf-8").replace("{SERIAL}", serial))
    return p


def power_cycle_port(serial):
    """The hub port that powers this probe, if the bench names one."""
    if os.environ.get("FRL_POWER_CYCLE"):
        return os.environ["FRL_POWER_CYCLE"]
    try:
        return (json.loads(SETTINGS.read_text(encoding="utf-8")).get("power_cycle") or {}).get(serial, "")
    except (OSError, ValueError):
        return ""


def python_runner():
    """CCStudio's Python runner, next to the JavaScript one toolfind locates."""
    rs = toolfind.find("ccs_scripting")
    py = Path(rs["exe"]).parent / "python" / ("run.bat" if os.name == "nt" else "run.sh")
    if not py.is_file():
        stop("refused", "FRL-E020", f"CCStudio's Python scripting is missing ({py})", "install CCStudio 20 or later (https://www.ti.com/tool/CCSTUDIO)")
    return str(py)


def uhubctl(args):
    """(exit, output) of one uhubctl call; a missing uhubctl is exit 127."""
    try:
        p = subprocess.run(["uhubctl", *args], capture_output=True, text=True, timeout=30)
        return p.returncode, p.stdout + p.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return 127, str(e)


def power_cycle(err, hub, serial, run=uhubctl, sleep=time.sleep):
    """Switch the hub port off and on, only for the refused connect (Error -242), only when uhubctl lists this probe's
    serial on that port, and only when each command succeeds. Returns (cycled, why not)."""
    if "-242" not in err:
        return False, "not the refused connect (-242) a power cycle fixes"
    if not re.fullmatch(r"[0-9.-]+:\d+", hub or ""):
        return False, "no hub port named"
    loc, port = hub.split(":")
    code, out = run(["-l", loc, "-p", port])
    line = next((ln for ln in out.splitlines() if re.match(r"\s*Port %s:" % port, ln)), "")
    if code != 0 or not line:
        return False, f"uhubctl does not list port {port} on hub {loc} (exit {code})"
    if serial not in line:
        other = re.search(r"CMSIS-DAP (\S+)\]", line)
        return False, f"hub port {hub} holds {other.group(1) if other else 'another device'}, not probe {serial}; the bench's port map is stale"
    code, out = run(["-l", loc, "-p", port, "-a", "off"])
    if code != 0:
        return False, f"uhubctl could not switch port {hub} off (exit {code})"
    sleep(6)                                              # 3 s did not reopen the window (cc1354p10/FAQ.md)
    for _ in range(2):
        code, out = run(["-l", loc, "-p", port, "-a", "on"])
        if code == 0:
            return True, ""
        sleep(1)
    return False, f"uhubctl could not switch port {hub} back on (exit {code}): the probe is off; run uhubctl -l {loc} -p {port} -a on"


def _once(board, serial, args, timeout):
    try:
        p = subprocess.run([python_runner(), str(SCRIPT), str(ccxml(board, serial)), *map(str, args)],
                           capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        return None, e.stdout if isinstance(e.stdout, str) else ""


def run(board, serial, args, timeout=600):
    """(exit, output) of one frl_cc13x4.py operation; after a -242 refusal, one guarded power cycle and one retry."""
    code, out = _once(board, serial, args, timeout)
    hub = power_cycle_port(serial)
    if code != 3 or "connect refused" not in (out or "") or not hub:
        return code, out
    ok, why = power_cycle(out, hub, serial, run=uhubctl)
    if not ok:
        return code, out + f"\nfatal: connect refused; no power cycle: {why}\n"
    loc, port = hub.split(":")
    for _ in range(40):                                   # the probe comes back on USB
        time.sleep(0.5)
        if serial in uhubctl(["-l", loc, "-p", port])[1]:
            break
    code, out2 = _once(board, serial, args, timeout)
    return code, f"CYCLED {hub}\n" + (out2 or "")


def cycled(out):
    m = re.search(r"^CYCLED (\S+)$", out or "", re.M)
    return m.group(1) if m else None


def _failed(out, serial, what):
    msg = next((ln.strip() for ln in (out or "").splitlines() if ln.startswith("fatal:")), "no answer")
    why = re.search(r"no power cycle: (.*)$", out or "", re.M)
    if why:
        msg = f"{msg[:160]}; no power cycle: {why.group(1)}"
    elif cycled(out):
        msg = f"{msg[:160]}; still refused after a power cycle of hub port {cycled(out)} (a hub that does not cut the port's power cannot reopen the window)"
    nxt = "check the board's power and USB cable; close CCStudio or UniFlash if they hold the probe; then try again"
    if "connect refused" in msg and not power_cycle_port(serial):
        nxt = ("this chip takes a debugger only just after power-on: unplug the probe's USB cable for 6 s, plug it back in and "
               "run the step again at once (or set FRL_POWER_CYCLE=<hub>:<port> for a hub uhubctl can switch)")
    stop("failed", "FRL-E006", f"CCStudio could not {what} the board on probe {serial}: {msg[:200]}", nxt, ccs=(out or "")[-600:])


def elf(path):
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        stop("refused", "FRL-E031", f"{path} is not a 32-bit little-endian ELF image", "rebuild it with `frl build`")
    return data


def main_segments(image):
    """[(address, bytes)] the image loads into main flash; its CCFG segment is not included."""
    data = elf(image)
    phoff, = struct.unpack_from("<I", data, 28)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    segs = []
    for k in range(phnum):
        p_type, off, _, paddr, filesz, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + k * phentsize)
        if p_type == 1 and filesz and paddr < MAIN_END:
            segs.append((paddr, data[off:off + filesz]))
    if not segs:
        stop("refused", "FRL-E034", f"{Path(image).name} loads nothing into main flash", "rebuild it with `frl build`")
    return sorted(segs)


def sectors(addr, n):
    """The sector-aligned range [start, end) a write of n bytes at addr erases."""
    return addr // SECTOR * SECTOR, (addr + n + SECTOR - 1) // SECTOR * SECTOR


def write_blocks(image):
    """[(address, bytes)]: the sectors the image touches, merged where they meet, each filled 0xFF around the
    image's bytes. These are exactly the bytes the write changes; nothing between blocks is erased."""
    blocks = []
    for a, b in main_segments(image):
        s, e = sectors(a, len(b))
        if blocks and s <= blocks[-1][1]:
            blocks[-1][1] = max(blocks[-1][1], e)
        else:
            blocks.append([s, e])
    out = []
    for s, e in blocks:
        buf = bytearray(b"\xff" * (e - s))
        for a, b in main_segments(image):
            if s <= a < e:
                buf[a - s:a - s + len(b)] = b
        out.append((s, bytes(buf)))
    return out


def block_files(image):
    """write_blocks as files, for loadBinaryProgram: [<bin>, <addr>, ...]."""
    d = Path(tempfile.mkdtemp(prefix="frl-ccs13-"))
    args = []
    for s, b in write_blocks(image):
        f = d / f"{Path(image).stem}-{s:08X}.bin"
        f.write_bytes(b)
        args += [f, f"0x{s:X}"]
    return args


def symbol(image, name):
    """Address of a global symbol in the image's ELF symbol table."""
    data = elf(image)
    shoff, = struct.unpack_from("<I", data, 32)
    shentsize, shnum = struct.unpack_from("<HH", data, 46)
    secs = [struct.unpack_from("<IIIIIIIIII", data, shoff + k * shentsize) for k in range(shnum)]
    for s in secs:
        if s[1] != 2:                               # SHT_SYMTAB
            continue
        strtab = secs[s[6]]
        for k in range(s[5] // 16):
            st_name, st_value = struct.unpack_from("<II", data, s[4] + k * 16)
            end = data.index(b"\0", strtab[4] + st_name)
            if data[strtab[4] + st_name:end].decode("ascii", "replace") == name:
                return st_value
    stop("refused", "FRL-E070", f"{Path(image).name} has no {name}", "the image must come from frl's own pattern project: run `frl prepare` and `frl build`")


def identity(board, serial):
    code, out = run(board, serial, ["identity"], timeout=180)
    m = re.search(r"^ID ([0-9A-F]{8}) ([0-9A-F]{8})$", out or "", re.M)
    if code != 0 or not m:
        _failed(out, serial, "read")
    return {"deviceid": f"0x{m.group(1)}", "user_id": f"0x{m.group(2)}", "power_cycled": cycled(out)}


def check_identity(board, serial):
    ident = identity(board, serial)
    i = board["target"]["identity"]
    mask = hexint(i.get("deviceid_mask", "0xFFFFFFFF"))
    accepted = [i["deviceid"]] + list(i.get("deviceid_any") or [])
    if not any(hexint(ident["deviceid"]) & mask == hexint(x) & mask for x in accepted):
        stop("refused", "FRL-E005", f"probe {serial} is attached to a chip with ICEPICK_DEVICE_ID {ident['deviceid']}, not the {board['kit']} ({', '.join(accepted)})",
             f"check the name printed on the board ({board['printed']}); pick the matching board, or the right --probe", identity=ident)
    return ident


def program(board, serial, image):
    code, out = run(board, serial, ["program", *block_files(image)])
    if code is None:
        stop("failed", "FRL-E033", "CCStudio stopped answering during programming; the board state is unknown",
             "do not write again blindly: run `frl restore` from the backup", ccs=(out or "")[-600:])
    if code != 0 or "OK programmed" not in out:
        _failed(out, serial, "program")
    return out


def verify_image(board, serial, image):
    code, out = run(board, serial, ["verify", *block_files(image)])
    if code != 0 or not re.search(r"^(OK verified|MISMATCH)", out or "", re.M):
        _failed(out, serial, "read")
    return "OK verified" in out, out


def start(board, serial):
    code, out = run(board, serial, ["start"], timeout=180)
    if code != 0 or "OK started" not in (out or ""):
        _failed(out, serial, "start")
    return code, out


def sample(board, serial, led, seconds, image=None):
    """Same shape as target.sample, from the program's toggle log: level 1 (lit) at even log entries."""
    if not image:
        stop("unverified", "FRL-E042", "this board's pattern check reads the program's own toggle log, so it needs the image",
             "run `frl verify` with --image <the image you flashed>")
    n_addr, log_addr = symbol(image, "frl_log_n"), symbol(image, "frl_log_ms")
    if "dio" not in led:
        stop("unverified", "FRL-E042", f"the route names no DIO for {led['label']}, so frl cannot check the pin itself", "this is a route bug: report it")
    code, out = run(board, serial, ["sample", f"0x{n_addr:X}", f"0x{log_addr:X}", 64, seconds, led["dio"]], timeout=180 + seconds)
    m = re.search(r"^N (\d+) (\d+)$", out or "", re.M)
    if code != 0 or not m:
        _failed(out, serial, "sample")
    n, span = map(int, m.groups())
    times = [int(t) for t in re.findall(r"^T (\d+)$", out, re.M)]
    first = n - len(times)
    t0 = times[0] if times else 0
    trans = [(t - t0, 1 if (first + k) % 2 == 0 else 0) for k, t in enumerate(times)]
    e = re.search(r"^E (\d|\?)$", out, re.M)
    o = re.search(r"^O (\d|\?)$", out, re.M)
    enabled = int(e.group(1)) if e and e.group(1) != "?" else None       # unread is unknown, never inferred from the log
    level = int(o.group(1)) if o and o.group(1) != "?" else None
    want = 1 if n and (n - 1) % 2 == 0 else 0                            # the program writes the pin, then logs it
    agrees = None if level is None or not n else level == want
    logged = (times[-1] - t0) if times else 0
    # The log is timed by the chip's own clock, to the ms: report it as one sample a ms over the logged span.
    return {"transitions": trans, "samples": max(logged, 1), "span_ms": max(logged, 1), "output_enabled": enabled,
            "pin_agrees": agrees, "power_cycled": cycled(out),
            "method": "the program's own toggle log, read after a halt, with the pin's own output-enable and output bits"}


def backup_region(board, serial, addr, nbytes, out):
    code, o = run(board, serial, ["backup", f"0x{addr:X}", nbytes, out])
    if code != 0 or "OK saved" not in o:
        _failed(o, serial, "read")


def restore_region(board, serial, image, addr):
    code, o = run(board, serial, ["restore", image, f"0x{addr:X}"])
    if code != 0 or "OK restored" not in o:
        _failed(o, serial, "restore")
