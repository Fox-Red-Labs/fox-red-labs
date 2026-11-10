"""Talk to one board through one selected probe with OpenOCD: identity, write limits, program, sample a pin.

Every call names the probe serial explicitly; nothing reads PROBE from the environment. Errors keep
OpenOCD's own words in the result so a person can search them.
"""
import re
import struct
import subprocess
from pathlib import Path

import toolfind
from core import hexint, stop

OPENOCD_BUSY = ("LIBUSB_ERROR_BUSY", "unable to claim", "Resource busy")
OPENOCD_DENIED = ("LIBUSB_ERROR_ACCESS", "Permission denied", "LIBUSB_ERROR_NOT_SUPPORTED")


def denied_next(plat=None):
    """The one step that lets this user open the probe, per OS (Linux needs a udev rule; Windows a driver)."""
    import sys as _sys
    plat = plat or _sys.platform
    if plat.startswith("linux"):
        return ("Linux lets only root open a new USB probe until a udev rule allows your user: "
                "sudo cp ~/xpack-openocd-0.12.0-7/openocd/contrib/60-openocd.rules /etc/udev/rules.d/ && "
                "sudo udevadm control --reload && sudo udevadm trigger, then unplug and replug the board "
                "(docs/install-routes.md, \"Linux: let your user open the probe\")")
    if plat == "win32":
        return ("Windows needs the XDS110 USB driver, which UniFlash or CCStudio installs; install one of them once and "
                "replug the board. This route has not been run on Windows yet (docs/install-routes.md, \"Your computer\")")
    return "unplug and replug the board, close any other debugger, then run `frl detect`"


# ---------------------------------------------------------------- the connect ladder
# When a board does not answer, frl climbs these steps, cheapest first, and stops at the first that works. Each try
# is kept with OpenOCD's own last lines so a report maps to a fix. Steps that erase are never on the ladder.
# The other wire protocol and the wake-from-sleep read live in discover.py and identity().

ATTACH_FAIL = ("Error connecting DP", "Could not find MEM-AP", "returned error -614", "returned error -613",
               "init mode failed", "Failed to read memory", "DP initialisation failed", "Error: Examination failed",
               "unable to connect", "Error: Invalid ACK", "SWD ack not OK", "JTAG scan chain interrogation failed",
               "Error: Target not examined")
SPEED_STEPS = (4000, 1000, 100)
PROBE_FW_MIN = "3.0.0.43"     # the version DSLite 21 asks XDS110 probes for (mspm0c1104/FAQ.md, f2800137 bring-up)
_SETTINGS = {}                # serial -> {"speed_khz", "under_reset"} once a step worked, for the rest of this process
_LOG = {}                     # serial -> [{"step", "ok", "words"}]


def speed_khz(board, serial):
    return _SETTINGS.get(serial, {}).get("speed_khz") or board["target"].get("speed_khz", 1000)


def attach_failed(out):
    """True when OpenOCD's words say it could not reach the chip (not a busy or locked-out probe, not a bad command)."""
    out = out or ""
    if any(s in out for s in OPENOCD_BUSY + OPENOCD_DENIED):
        return False
    return any(s in out for s in ATTACH_FAIL)


def _words(out, n=3):
    keep = [l.strip() for l in (out or "").splitlines() if l.startswith(("Error", "Warn")) or "error" in l]
    return keep[-n:] or [l.strip() for l in (out or "").splitlines() if l.strip()][-1:]


def steps(board, serial):
    """The ladder for this board, in order: (name, apply) where apply() changes settings or acts on the probe and
    returns a note, or None when the step does not apply here."""
    t = board["target"]
    out = []
    if t.get("connect_under_reset", True):      # false on MSPM0: memory access waits for nRST (TRM Table 22-2)
        out.append(("connect under reset", lambda: _SETTINGS.setdefault(serial, {}).update(under_reset=True) or "nRST held through init"))
    for k in SPEED_STEPS:
        if k < t.get("speed_khz", 1000):
            out.append((f"clock {k} kHz", lambda k=k: _SETTINGS.setdefault(serial, {}).update(speed_khz=k, under_reset=False) or f"adapter speed {k}"))
    out.append(("USB port power cycle", lambda: usb_cycle(board, serial)))
    return out


def attach(board, serial, cmds, ok, timeout=60, start=None):
    """run() with the ladder: on a failure to reach the chip, try each step until ok(code, out) holds.
    start names one step to run alone (a hardware test of that step); the plain try is skipped then."""
    log = _LOG.setdefault(serial, [])
    if start is None:
        code, out = run(board, serial, cmds, timeout)
        if ok(code, out) or not attach_failed(out):
            return code, out
        log.append({"step": "as configured", "ok": False, "words": _words(out)})
    ladder = steps(board, serial)
    if start is not None:
        names = [n for n, _ in ladder]
        if start not in names:
            stop("refused", "FRL-E007", f"no connect step named {start!r} for this board", "pick one of: " + ", ".join(names))
        ladder = [ladder[names.index(start)]]
    code, out = None, ""
    for name, apply in ladder:
        note = apply()
        if note is None:
            log.append({"step": name, "ok": None, "words": ["does not apply here"]})
            continue
        code, out = run(board, serial, cmds, timeout)
        good = bool(ok(code, out))
        log.append({"step": name, "ok": good, "note": note, "words": [] if good else _words(out)})
        if good:
            _SETTINGS.setdefault(serial, {})["step"] = name
            return code, out
    return code, out


def connect_note(serial):
    """What the ladder did on this probe in this process, for a run's result; None when the first try worked."""
    log = _LOG.get(serial)
    if not log:
        return None
    return {"worked": _SETTINGS.get(serial, {}).get("step"), "tried": log}


def _xdsdfu(args, timeout=30):
    x = toolfind.find("xdsdfu", required=False)
    if not x or not x.get("exe"):
        return None
    try:
        return subprocess.run([x["exe"]] + args, capture_output=True, text=True, timeout=timeout).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None


def parse_xdsdfu(out):
    """[{serial, version, mode}] from `xdsdfu -e`."""
    devs = []
    for blk in re.split(r"<<<< Device \d+ >>>>", out or "")[1:]:
        g = lambda k: (re.search(rf"{k}:\s*(\S+)", blk) or [None, None])[1]
        devs.append({"serial": g("Serial Num"), "version": g("Version"), "mode": g("Mode")})
    return devs


def _ver(v):
    return tuple(int(x) for x in re.findall(r"\d+", v or "0"))


def probe_firmware(serial, fw_min=PROBE_FW_MIN):
    """The XDS110's firmware version against fw_min, read-only (`xdsdfu -e`), for a failure report; None without xdsdfu.
    There is no software reset of the probe on its own: xdsdfu leaves its boot loader only as part of a firmware,
    serial or configuration write (`-r` alone answers "No file name was specified"; tried 2026-10-24)."""
    d = next((d for d in parse_xdsdfu(_xdsdfu(["-e"])) if d["serial"] == serial), None)
    if not d:
        return None
    return {"version": d["version"], "minimum": fw_min, "below": _ver(d["version"]) < _ver(fw_min)}


def usb_cycle(board, serial, recovery=None):
    """Cut and restore power on the hub port this board's probe is on, with uhubctl. Only on a port whose power
    cut is recorded as measured ("power_cut": "yes" in data/recovery.json); None otherwise."""
    import json as _json
    import shutil
    import time
    if recovery is None:
        p = Path(__file__).resolve().parents[2] / "data" / "recovery.json"
        recovery = _json.loads(p.read_text(encoding="utf-8")) if p.is_file() else {}
    from core import routes
    bid = board.get("id") or next((k for k, v in routes()["boards"].items() if v.get("kit") == board.get("kit")), None)
    port = next(((h, q) for h in recovery.get("hubs", []) for q in h.get("ports", [])
                 if q.get("board") == bid and q.get("power_cut") == "yes"), None)
    exe = shutil.which("uhubctl")
    if not port or not exe:
        return None
    h, q = port
    loc = q.get("location") or h["location"]
    subprocess.run([exe, "-l", loc, "-p", str(q["port"]), "-a", "cycle", "-d", "5"], capture_output=True, text=True, timeout=60)
    t0 = time.time()
    while time.time() - t0 < 20:
        if any(d["serial"] == serial for d in parse_xdsdfu(_xdsdfu(["-e"]))):
            break
        time.sleep(1)
    return f"uhubctl cycled hub {loc} port {q['port']} for 5 s; probe back after {time.time() - t0:.1f} s"


def openocd_args(board, serial):
    t = board["target"]
    kind = t.get("openocd", "openocd")          # "ti_openocd": TI's fork, for the parts xPack OpenOCD has no driver for
    ocd = toolfind.find(kind)
    if not ocd.get("scripts") or not (Path(ocd["scripts"]) / t["target_cfg"]).is_file():
        stop("refused", "FRL-E021", f"OpenOCD at {ocd['exe']} has no {t['target_cfg']}",
             f"install {toolfind.install_line(kind)}, or set {toolfind.ENV[kind]} to one that has it", tool=kind)
    args = [ocd["exe"]]
    if ocd.get("scripts"):
        args += ["-s", ocd["scripts"]]
    args += ["-c", "gdb_port disabled", "-c", "tcl_port disabled", "-c", "telnet_port disabled",
             "-f", t["interface_cfg"], "-c", f"adapter serial {serial}", "-c", f"adapter speed {speed_khz(board, serial)}",
             "-f", t["target_cfg"]]
    if _SETTINGS.get(serial, {}).get("under_reset"):
        args += ["-c", "reset_config srst_only srst_nogate connect_assert_srst"]
    return args, ocd


def run(board, serial, cmds, timeout=120, halt_ok=True):
    """Run OpenOCD commands after init. Returns (exit, combined output). Under the connect-under-reset rung the
    session holds nRST through init, so it starts by releasing it: halted at once when the caller may halt the core."""
    args, _ = openocd_args(board, serial)
    args += ["-c", "init"]
    if _SETTINGS.get(serial, {}).get("under_reset"):
        # Release nRST, then halt at once: on TI's MSPM33 and AM13 a `reset halt` after a held nRST times out
        # (the reset clears the vector catch), while a halt right after the release catches the program early.
        # MSPM33 keeps its memory port closed while nRST is held, so the target is examined again after the release.
        cmds = ["adapter deassert srst", "sleep 10", "catch {[target current] arp_examine}"] + (
            ["halt 500"] if halt_ok else ["sleep 100"]) + list(cmds)
        if halt_ok and not cmds[-1].startswith(("reset", "resume")):
            cmds.append("resume")
    for c in cmds:
        args += ["-c", c]
    args += ["-c", "shutdown"]
    try:
        import os   # PROBE matches the serial named, so nothing downstream can reach another probe through it
        r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout,
                           env=dict(os.environ, PROBE=serial))
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or "") if isinstance(e.stdout, str) else ""
    return r.returncode, r.stdout


def _attach_failure(out, serial):
    tried = _LOG.get(serial) or None
    if any(s in out for s in OPENOCD_DENIED):
        stop("failed", "FRL-E006", f"this user may not open probe {serial} (OpenOCD: access denied)",
             denied_next(), openocd=out[-600:])
    if any(s in out for s in OPENOCD_BUSY):
        stop("failed", "FRL-E004", f"probe {serial} could not be opened; another program (CCStudio, UniFlash or another OpenOCD) may hold it",
             "close the other program's debug session, then try again", openocd=out[-600:])
    nxt = "check the board's power LED and the debug jumpers, unplug and replug USB, then run `frl identify --probe <serial>`"
    fw = probe_firmware(serial) if tried else None
    if fw and fw["below"]:
        nxt = (f"the probe's firmware is {fw['version']}, below {fw['minimum']}: update it with xdsdfu "
               "(docs/frl-errors.md, FRL-E006), then try again")
    elif tried:
        nxt = ("frl tried every connect step that erases nothing (listed in 'connect'); unplug USB for 30 s, then try again. "
               "If it still fails, the board's FAQ section \"When the board stops answering\" names the recovery that erases; "
               "frl never runs it on its own")
    stop("failed", "FRL-E006", f"OpenOCD could not attach to the board on probe {serial}", nxt,
         openocd=out[-600:], connect=tried, probe_firmware=fw)


PROBE_LINE = re.compile(r"Device: (\S+), Flash: (\d+)kb, RAM: (\d+)kb")


def parse_flash_probe(out):
    """{part, flash_kb, ram_kb} from TI's cc23xx/cc27xx driver line "Device: CC2340R52E0RKPR, Flash: 512kb, RAM: 36kb", or None."""
    m = PROBE_LINE.search(out or "")
    return {"part": m.group(1), "flash_kb": m.group(2), "ram_kb": m.group(3)} if m else None


def identity_flash_probe(board, serial, start=None):
    """The radio parts (CC23xx, CC27xx): a plain read fails until TI's driver opens the debug port through SACI, so
    `flash probe 0` names the part. It halts the core in the boot ROM; `reset run` starts the application again.
    Writes nothing."""
    code, out = attach(board, serial, ["flash probe 0", "reset run"], lambda c, o: c == 0 and parse_flash_probe(o), timeout=60, start=start)
    got = parse_flash_probe(out)
    if code != 0 or not got:
        _attach_failure(out or "", serial)
    return got


def identity(board, serial, start=None):
    """{traceid, deviceid, userid} as hex strings, read-only; the radio parts give {part, flash_kb, ram_kb}."""
    if board["target"].get("transport") == "ccs13":
        import ccs13
        return ccs13.identity(board, serial)
    i = board["target"]["identity"]
    if i.get("method") == "flash_probe":
        return identity_flash_probe(board, serial, start) if start else identity_flash_probe(board, serial)
    rx = rf"0x{hexint(i['addr']):08x}:((?:\s+[0-9a-f]{{8}})+)"
    code, out = attach(board, serial, [f"mdw {i['addr']} {i['words']}"], lambda c, o: c == 0 and re.search(rx, o or ""), timeout=60, start=start)
    m = re.search(rx, out or "")
    if code != 0 or not m:
        _attach_failure(out or "", serial)
    words = m.group(1).split()
    if not any(int(w, 16) for w in words):
        # All zeros: the running program has the chip in a low-power state where the factory region does not answer
        # (LP-MSPM0G3519, 2026-10-24). A reset and halt writes nothing; the program runs again after it.
        code, out = run(board, serial, ["reset halt", f"mdw {i['addr']} {i['words']}", "reset run"], timeout=60)
        m = re.search(rx, out or "")
        if code != 0 or not m:
            _attach_failure(out or "", serial)
        words = m.group(1).split()
    return {f: f"0x{w.upper()}" for f, w in zip(i["fields"], words)}


def check_identity(board, serial, start=None):
    """The board behind this probe is the board named, or a Stop before any write. start: see attach()."""
    if board["target"].get("transport") == "dss":   # C2000: CCStudio scripting (dss.py)
        import dss
        return dss.check_identity(board, serial)
    if board["target"].get("transport") == "ccs13":  # CC13x4, CC26x4: CCStudio Python scripting (ccs13.py)
        import ccs13
        return ccs13.check_identity(board, serial)
    if board["target"].get("transport") == "toolbox":  # CC35xx: configuration-port ID, then TI's Wi-Fi Toolbox (toolbox.py)
        import toolbox
        return toolbox.check_identity(board, serial)
    ident = identity(board, serial, start) if start else identity(board, serial)
    i = board["target"]["identity"]
    if i.get("method") == "flash_probe":
        if not ident["part"].upper().startswith(i["part"].upper()):
            stop("refused", "FRL-E005",
                 f"probe {serial} is attached to a {ident['part']}, not the {board['kit']} ({i['part']})",
                 f"check the name printed on the board ({board['printed']}); pick the matching board, or the right --probe",
                 identity=ident)
        return ident
    got = hexint(ident["deviceid"]) & hexint(i["deviceid_mask"])
    accepted = [i["deviceid"]] + list(i.get("deviceid_any") or [])      # deviceid_any: more IDs the same kit may carry
    wants = {hexint(a) & hexint(i["deviceid_mask"]) for a in accepted}
    if got not in wants:
        stop("refused", "FRL-E005",
             f"probe {serial} is attached to a chip with DEVICEID {ident['deviceid']}, not the {board['kit']} ({' or '.join(accepted)})",
             f"check the name printed on the board ({board['printed']}); pick the matching board, or the right --probe",
             identity=ident)
    return ident


def elf_segments(path):
    """[(load address, bytes)] for an ELF image's loaded segments (32-bit little-endian Arm)."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        stop("refused", "FRL-E031", f"{path} is not a 32-bit little-endian ELF image", "rebuild it with `frl build`")
    phoff, = struct.unpack_from("<I", data, 28)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    segs = []
    for k in range(phnum):
        p_type, _, _, p_paddr, p_filesz, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + k * phentsize)
        if p_type == 1 and p_filesz:
            segs.append((p_paddr, p_filesz))
    return segs


def check_writes(board, image):
    """Every byte the image loads lands in a region this route may write, or a Stop before any write."""
    blocked = board["target"].get("write_blocked")
    if blocked:
        stop("refused", "FRL-E035", f"this route does not write {board['kit']}: {blocked}",
             "back up, build and check run; the write waits for a driver that erases and writes main flash only")
    allowed = [(hexint(w["start"]), hexint(w["end"]), w["name"]) for w in board["target"]["writable"]]
    kept = [(hexint(w["start"]), hexint(w["end"])) for w in board["target"].get("not_written", [])]
    segs = [(a, n) for a, n in elf_segments(image) if not any(s <= a and a + n <= e for s, e in kept)]
    for addr, n in segs:
        if not any(s <= addr and addr + n <= e for s, e, _ in allowed):
            stop("refused", "FRL-E034", f"the image writes {n} bytes at 0x{addr:08X}, outside {', '.join(nm for _, _, nm in allowed)}",
                 f"this route never writes there ({board['target']['never_write']})", segments=segs)
    if board["target"].get("transport") == "ccs13":     # the write erases whole sectors: name them, and check them too
        import ccs13
        out = []
        for a, n in segs:
            s, e = ccs13.sectors(a, n)
            if not any(lo <= s and e <= hi for lo, hi, _ in allowed):
                stop("refused", "FRL-E034", f"writing {n} bytes at 0x{a:08X} erases 0x{s:08X}-0x{e:08X}, outside {', '.join(nm for _, _, nm in allowed)}",
                     f"this route never writes there ({board['target']['never_write']})", segments=segs)
            out.append({"addr": f"0x{a:08X}", "bytes": n, "erases": f"0x{s:08X}-0x{e:08X}"})
        return out
    return [{"addr": f"0x{a:08X}", "bytes": n} for a, n in segs]


def write_path(board):
    """Which tool writes this board now: the route's first path, or DSLite (dslite_path.chosen)."""
    import dslite_path
    if dslite_path.chosen(board):
        return "dslite"
    t = board["target"]
    return "dslite" if t.get("write") == "dslite_lpf3" else ("ccstudio scripting" if t.get("transport") in ("dss", "ccs13") else "wi-fi toolbox" if t.get("transport") == "toolbox" else t.get("openocd", "openocd"))


def program(board, serial, image, timeout=120):
    import dslite_path
    if dslite_path.chosen(board):
        check_writes(board, image)
        return dslite_path.program(board, serial, image)
    if board["target"].get("transport") == "dss":
        import dss
        return dss.program(board, serial, image)
    if board["target"].get("transport") == "ccs13":
        import ccs13
        return ccs13.program(board, serial, image)
    if board["target"].get("transport") == "toolbox":
        import toolbox
        return toolbox.program(board, serial, image)
    if board["target"].get("write_blocked"):
        check_writes(board, image)      # stops with FRL-E035 before the probe is opened
    if board["target"].get("write") == "dslite_lpf3":     # CC23xx, CC27xx: TI's chip-erase write (lpf3.py)
        import lpf3
        return lpf3.program(board, serial, image)
    code, out = run(board, serial, ["reset halt", f"program {{{image}}} verify", "reset run"], timeout=timeout)
    if code is None:
        stop("failed", "FRL-E033", "OpenOCD stopped answering during programming; the board state is unknown",
             "do not flash again blindly: run `frl verify-image` to see what is on the board, or `frl restore`",
             openocd=out[-600:] if out else "")
    if code != 0 or "Verified OK" not in out:
        if "Programming Started" in out and "Programming Finished" not in out:
            stop("failed", "FRL-E033", "programming started and did not finish; the board state is unknown",
                 "check the USB cable, then run `frl verify-image` before anything else", openocd=out[-800:])
        if "Programming Started" not in out:
            _attach_failure(out, serial)
        stop("failed", "FRL-E032", "programming did not verify", "run `frl verify-image`; if it differs, `frl restore`",
             openocd=out[-800:])
    return out


def write_report(out):
    """How OpenOCD wrote and checked an image, from its own lines: write and verify times, and which verify path it
    took (a CRC run on the chip, or a byte-by-byte compare after the CRC failed or timed out)."""
    out = out or ""
    if "Writing binary size of" in out:               # TI's Wi-Fi Toolbox (CC35xx): it writes, and has no read-back
        import toolbox
        return toolbox.write_report(out)
    m_w = re.search(r"wrote (\d+) bytes .*? in ([\d.]+)s", out)
    m_v = re.search(r"verified (\d+) bytes in ([\d.]+)s", out)
    slow = [w for w in ("checksum mismatch - attempting binary compare", "timeout waiting for algorithm",
                        "error executing cortex_m crc algorithm", "Failed to run CRC", "crc algorithm") if w.lower() in out.lower()]
    return {"write_s": float(m_w.group(2)) if m_w else None, "verify_s": float(m_v.group(2)) if m_v else None,
            "verify_path": ("binary compare" if slow else "crc" if m_v or "Verified OK" in out
                            else "DSLite fast verification" if "Loading Program" in out and "Success" in out else None),
            "words": slow}


def start(board, serial):
    """Let the program run from a clean reset."""
    if board["target"].get("write") == "dslite_lpf3":
        import lpf3
        return lpf3.start(board, serial)
    if board["target"].get("transport") == "ccs13":
        import ccs13
        return ccs13.start(board, serial)
    if board["target"].get("transport") == "toolbox":
        import toolbox
        return toolbox.start(board, serial)
    if board["target"].get("transport") == "dss":
        import dss
        code, out = dss.run(board, serial, ["start"], timeout=120)
        if code != 0 or "OK started" not in (out or ""):
            dss._failed(out, serial, "start")
        return out
    return run(board, serial, ["reset run"], timeout=30)


def verify_image(board, serial, image):
    """True when the board's flash holds exactly this image's loaded bytes (read-only compare)."""
    import dslite_path
    if dslite_path.chosen(board):
        return dslite_path.verify_image(board, serial, image)
    if board["target"].get("transport") == "dss":
        import dss
        return dss.verify_image(board, serial, image)
    if board["target"].get("transport") == "ccs13":
        import ccs13
        return ccs13.verify_image(board, serial, image)
    if board["target"].get("transport") == "toolbox":     # no read-back exists: not known, rather than a pass
        return None, "the Wi-Fi Toolbox has no read-back, so the flash cannot be compared with the image"
    if board["target"].get("write") == "dslite_lpf3":
        import lpf3
        return lpf3.verify_image(board, serial, image)
    code, out = run(board, serial, ["halt", f"verify_image {{{image}}}", "resume"], timeout=120)
    if code is None or ("verified" not in out and "checksum mismatch" not in out.lower() and "contents differ" not in out):
        _attach_failure(out or "", serial)
    return code == 0 and "contents differ" not in out and "verified" in out, out


def sample(board, serial, led, seconds, image=None):
    """[(ms, level)] transitions of one LED pin, plus samples taken and the pin's output-enable bit. image: the
    image on the board, for a route that times the pattern from the program's own log (ccs13)."""
    if board["target"].get("transport") == "ccs13":
        import ccs13
        return ccs13.sample(board, serial, led, seconds, image)
    if board["target"].get("transport") == "dss":
        import dss
        return dss.sample(board, serial, led, seconds)
    if board["target"].get("write") == "dslite_lpf3":
        import lpf3
        return lpf3.sample(board, serial, led, seconds)
    base = hexint(led["gpio_base"])
    off = board["gpio_offsets"]
    dout, doe, bit = base + hexint(off["dout"]), base + hexint(off["doe"]), int(led["bit"])
    tcl = (f"set e [read_memory 0x{doe:08X} 32 1]; echo \"E [expr {{($e >> {bit}) & 1}}]\"; "
           f"set t0 [ms]; set last -1; set n 0; "
           f"while {{[expr {{[ms] - $t0}}] < {int(seconds * 1000)}}} {{ "
           f"set v [read_memory 0x{dout:08X} 32 1]; set b [expr {{($v >> {bit}) & 1}}]; incr n; "
           f"if {{$b != $last}} {{ echo \"T [expr {{[ms] - $t0}}] $b\"; set last $b }} }}; "
           f"echo \"N $n [expr {{[ms] - $t0}}]\"")
    code, out = run(board, serial, [tcl], timeout=60 + seconds, halt_ok=False)
    if code != 0 or "N " not in (out or ""):
        _attach_failure(out or "", serial)
    trans = [(int(t), int(b)) for t, b in re.findall(r"^T (\d+) (\d)$", out, re.M)]
    n, span = map(int, re.search(r"^N (\d+) (\d+)$", out, re.M).groups())
    enabled = int(re.search(r"^E (\d)$", out, re.M).group(1))
    return {"transitions": trans, "samples": n, "span_ms": span, "output_enabled": enabled}


def console_port(board, serial):
    """The board's application serial port for this probe, or a Stop (not verified) where it cannot be found."""
    import glob
    import sys as _sys
    c = board.get("console") or {}
    if _sys.platform == "win32":
        hits = windows_com_ports(serial)
    else:
        key = "mac" if _sys.platform == "darwin" else "linux"
        pat = c.get(key, "").replace("{serial}", serial)
        hits = sorted(glob.glob(pat)) if pat else []
    if not hits:
        nxt = "open the board's first USB serial port at 115200 baud in a terminal program and look for your message"
        if _sys.platform.startswith("linux") and glob.glob("/dev/ttyACM*"):
            nxt += ("; if opening /dev/ttyACM0 says Permission denied, add yourself to the dialout group: "
                    "sudo usermod -aG dialout $USER, then log out and back in")
        stop("unverified", "FRL-E042", f"could not find the serial port for probe {serial}", nxt)
    return hits[0]


def windows_com_ports(serial, listing=None):
    """COM ports of the XDS110 behind this serial on Windows: its interface entries carry the parent's
    serial in their container, so match the 'Application/User UART' name and the serial when Windows shows
    it, else the single XDS110 UART when only one probe is plugged in. Written from TI's driver names;
    not run on a Windows machine yet."""
    import json as _json
    import re as _re
    if listing is None:
        ps = ("Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*XDS110*UART*' } | "
              "Select-Object Name, DeviceID, PNPDeviceID | ConvertTo-Json")
        try:
            out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=30).stdout
            listing = _json.loads(out or "[]")
        except (OSError, ValueError, subprocess.TimeoutExpired):
            listing = []
    items = listing if isinstance(listing, list) else [listing]
    ports = []
    for it in items:
        m = _re.search(r"\((COM\d+)\)", it.get("Name") or "")
        if m and "Application/User" in (it.get("Name") or ""):
            ports.append((m.group(1), serial.upper() in (it.get("PNPDeviceID") or "").upper()))
    mine = [p for p, has in ports if has]
    return mine or ([ports[0][0]] if len(ports) == 1 else [])
