"""The "toolbox" transport: frl's identity, write and start for a CC35xx (LP-EM-CC35X1) through TI's SimpleLink Wi-Fi
Toolbox. target.py and ops.py call these when a route says "transport": "toolbox".

What this chip allows: the program sits in external flash behind the chip's security, and the Toolbox has no
read-back, so frl cannot back it up or compare it with an image. The backup records the chip's fuse data, its unique
ID and what the running program prints, and keeps a hashed copy of a signed image the person names (TI's or their own
build); the restore programs that copy only. The check is the program's own output on the probe's serial port. A
write never activates the chip, erases it or changes its fuses.

What the files must prove, since the board cannot (Codex review 2026-10-25): the signed vendor image holds every byte
of the .out it is programmed for; what is programmed is a copy frl hashed, never a build folder that can change
under it; the flash layout of the image (one slot or two) is the layout frl last put on this chip, or the person says
what is there; a failed reset is reported, not taken for a started program.
"""
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
from pathlib import Path

import toolfind
from core import data_dir, stop

FLASH_TYPE = "IS25WJ064F"          # the LP-EM-CC35X1's QSPI flash (cc35x1/AGENTS.md)


def toolbox():
    return toolfind.find("wifi_toolbox")


def run(serial, args, timeout=600):
    """(exit, output) of one Toolbox programmer command, run from the Toolbox folder (its scripts resolve paths there)."""
    tb = toolbox()
    try:
        p = subprocess.run([tb["exe"], "programmer", "-i", "XDS110", "-param1", serial, *map(str, args)],
                           capture_output=True, text=True, timeout=timeout, cwd=tb["path"])
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        return None, e.stdout if isinstance(e.stdout, str) else ""


def _failed(out, serial, what):
    err = next((ln.strip() for ln in reversed((out or "").splitlines()) if "error" in ln.lower()), "no answer")
    stop("failed", "FRL-E006", f"the Wi-Fi Toolbox could not {what} the board on probe {serial}: {err[:200]}",
         "check the board's own USB-C power cable and the probe's TGT VDD jumper on EXT; if it says -615, unplug the "
         "board's USB-C for 5 s and try again; close CCStudio if it holds the probe", toolbox=(out or "")[-600:])


def fuse_data(serial):
    """The Toolbox's read-only fuse report (life cycle, keys, rollback counters), as text."""
    code, out = run(serial, ["get_fuse_data", "--flash_type", FLASH_TYPE, "--activation_type", "sdk_example_key", "--verbose"], timeout=180)
    if code != 0 or "Waiting for response from device" not in (out or ""):
        _failed(out, serial, "read")
    return out


def udid(serial):
    """The chip's unique ID from its configuration port (UNIQUE_DEVICE_ID0 and 1), read-only, or None."""
    import discover
    r = discover.config_port_regs(serial, (0x28, 0x2C))
    if 0x28 not in r or 0x2C not in r or (r[0x28] == 0 and r[0x2C] == 0):
        return None
    return f"{r[0x28]:08X}-{r[0x2C]:08X}"


def check_identity(board, serial):
    """The configuration port's DEVICEID names the part and its unique ID names the chip (discover.py, read-only)."""
    import discover
    got = discover.identify(serial)
    want = board["target"]["identity"]["part"]
    if want not in (got.get("part") or []):
        stop("refused", "FRL-E005", f"probe {serial} is attached to {', '.join(got.get('part') or []) or 'no chip that answered'}, not the {board['kit']} ({want})",
             f"check the name printed on the board ({board['printed']}); pick the matching board, or the right --probe", identity=got)
    uid = udid(serial)
    if not uid:
        stop("failed", "FRL-E006", f"probe {serial} reached the {want} but not its unique ID, so frl cannot tell this chip from another",
             "unplug the board's USB-C for 5 s and try again; close CCStudio if it holds the probe")
    return {"part": want, "how": got.get("how"), "udid": uid}


def settings_for(image):
    """toolbox/tool_settings.json beside a built .out, or the file itself."""
    p = Path(image).expanduser()
    s = p if p.name == "tool_settings.json" else p.parent / "toolbox" / "tool_settings.json"
    if not s.is_file():
        stop("refused", "FRL-E031", f"no signed images for {p.name} (looked for {s})", "build it with `frl build`; the build signs it")
    return s


# ---------------------------------------------------------------- the signed bundle: what is programmed

def _sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def referenced(ts):
    """Every file path a tool_settings.json names (empty entries and nulls are unused slots)."""
    if isinstance(ts, dict):
        return [p for v in ts.values() for p in referenced(v)]
    return [ts] if isinstance(ts, str) and ts else []


def _segments(image):
    """[(address, bytes)] of an ELF's loaded segments (target.elf_segments, with the bytes)."""
    data = Path(image).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        stop("refused", "FRL-E031", f"{image} is not a 32-bit little-endian ELF image", "rebuild it with `frl build`")
    phoff, = struct.unpack_from("<I", data, 28)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    out = []
    for k in range(phnum):
        t, off, _, pa, fs, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + k * phentsize)
        if t == 1 and fs:
            out.append((pa, data[off:off + fs]))
    return sorted(out)


def check_from_build(image, settings):
    """The signed vendor image holds every loaded byte of the .out, each segment at its distance from the first
    (true of the Toolbox's image builder, 2026-10-25), so a stale or swapped bundle beside a new .out is refused."""
    ts = json.loads(Path(settings).read_text(encoding="utf-8"))
    vend = (ts.get("programming_debug_and_ota_signed_components") or {}).get("primary_vendor_image")
    segs = _segments(image)
    if not vend or not Path(vend).is_file() or not segs:
        stop("refused", "FRL-E031", f"{Path(settings).name} names no primary vendor image to check against {Path(image).name}",
             "build and sign it again with `frl build`")
    blob, a0 = Path(vend).read_bytes(), segs[0][0]
    at, ok = blob.find(segs[0][1]), False
    while at >= 0 and not ok:             # every place the first segment appears, until all segments line up
        ok = all(blob[at + a - a0:at + a - a0 + len(b)] == b for a, b in segs)
        at = blob.find(segs[0][1], at + 1)
    if not ok:
        stop("refused", "FRL-E031", f"the signed images beside {Path(image).name} are not from this build (the vendor image does not hold its bytes)",
             "build and sign it again with `frl build`; never copy a toolbox folder from another build")


def layout_of(build_dir):
    """The flash layout a build expects, from its external_memory_configurator.json: {"ota", "slots"}, or None."""
    f = Path(build_dir) / "external_memory_configurator.json"
    if not f.is_file():
        return None
    j = json.loads(f.read_text(encoding="utf-8"))
    return {"ota": bool((j.get("internal") or {}).get("ota")), "slots": j.get("inputs") or {}}


def _layout_name(lay):
    return "the two-slot OTA layout" if lay and lay["ota"] else "the one-slot layout" if lay else "an unknown layout"


def copy_bundle(settings, dest):
    """Copy tool_settings.json, every file it names and the build's layout file into dest, point the copy's settings
    at the copied files, and hash them. Returns the manifest that check_copy holds the copy to."""
    settings = Path(settings)
    ts = json.loads(settings.read_text(encoding="utf-8"))
    files = referenced(ts)
    missing = [f for f in files if not Path(f).is_file()]
    if missing:
        stop("refused", "FRL-E031", f"{settings.name} names {len(missing)} file(s) that are not there: {', '.join(Path(f).name for f in missing[:3])}",
             "build and sign it again with `frl build`")
    names = [Path(f).name for f in files]
    if len(set(names)) != len(set(files)):
        stop("refused", "FRL-E031", f"{settings.name} names two different files with the same name", "build and sign it again with `frl build`")
    dest = Path(dest)
    dest.mkdir(parents=True, exist_ok=True)
    dest = dest.resolve()
    for f in set(files):
        shutil.copyfile(f, dest / Path(f).name)

    def point(v):
        if isinstance(v, dict):
            return {k: point(x) for k, x in v.items()}
        return str(dest / Path(v).name) if isinstance(v, str) and v else v
    (dest / "tool_settings.json").write_text(json.dumps(point(ts), indent=4) + "\n", encoding="utf-8")
    lay_file = settings.parent.parent / "external_memory_configurator.json"
    if lay_file.is_file():
        shutil.copyfile(lay_file, dest / lay_file.name)
    hashes = {p.name: _sha(p) for p in sorted(dest.iterdir()) if p.is_file()}
    def named(v):        # the settings with each path cut to its file name: the bundle's hash is the same in any folder
        return {k: named(x) for k, x in v.items()} if isinstance(v, dict) else (Path(v).name if isinstance(v, str) and v else v)
    ident = dict((k, h) for k, h in hashes.items() if k != "tool_settings.json")
    ident["tool_settings.json"] = hashlib.sha256(json.dumps(named(ts), sort_keys=True).encode()).hexdigest()
    digest = hashlib.sha256(json.dumps(ident, sort_keys=True).encode()).hexdigest()
    return {"files": hashes, "sha256": digest, "layout": layout_of(dest), "from": str(settings)}


def check_copy(dest, man):
    """The copy is byte for byte what copy_bundle hashed, and its settings name only files inside it."""
    dest = Path(dest).resolve()
    have = {p.name: _sha(p) for p in sorted(dest.iterdir()) if p.is_file()} if dest.is_dir() else {}
    if have != man.get("files"):
        bad = sorted(set(have) ^ set(man.get("files") or {})) + sorted(k for k in have if k in man.get("files", {}) and have[k] != man["files"][k])
        stop("refused", "FRL-E052", f"the saved copy of the signed images changed since it was made ({', '.join(bad[:3]) or 'missing'})",
             "do not program it; take a new backup with a fresh FRL_RESTORE_IMAGE")
    for f in referenced(json.loads((dest / "tool_settings.json").read_text(encoding="utf-8"))):
        if Path(f).resolve().parent != dest:
            stop("refused", "FRL-E052", f"the saved tool_settings.json names a file outside its copy ({f})", "take a new backup")
    return dest / "tool_settings.json"


# ---------------------------------------------------------------- the flash layout frl last put on each chip

def _layouts():
    return data_dir() / "cc35-layouts.json"


def known_layout(uid):
    try:
        return json.loads(_layouts().read_text(encoding="utf-8")).get(uid)
    except (OSError, ValueError):
        return None


def record_layout(uid, lay):
    try:
        d = json.loads(_layouts().read_text(encoding="utf-8"))
    except (OSError, ValueError):
        d = {}
    d[uid] = lay
    tmp = _layouts().with_suffix(".tmp")
    tmp.write_text(json.dumps(d, indent=1) + "\n", encoding="utf-8")
    os.replace(tmp, _layouts())


STATED = {"ota": True, "single": False}


def check_layout(uid, lay):
    """Refuse a write whose layout differs from the one on the chip. The board cannot be read back, so the layout is
    the one frl last programmed on this chip, or what the person states with FRL_CC35_LAYOUT (ota, single, or erased
    after they ran TI's full_flash_erase themselves). frl never erases this chip."""
    if not lay:
        stop("refused", "FRL-E036", "this image's build has no external_memory_configurator.json, so frl cannot tell which flash layout it needs",
             "program it with TI's Toolbox yourself, or use a build made with `frl build`")
    said = (os.environ.get("FRL_CC35_LAYOUT") or "").strip().lower()
    if said and said not in ("ota", "single", "erased"):
        stop("refused", "FRL-E070", f"FRL_CC35_LAYOUT={said} is not ota, single or erased")
    have = known_layout(uid)
    erase = ("run TI's erase yourself, `simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe> full_flash_erase "
             "--flash_type AUTO_DETECT --activation_type sdk_example_key` (it removes the program on the board), then run again with FRL_CC35_LAYOUT=erased")
    if said == "erased":
        return "stated erased"
    if have is not None:
        if said and STATED[said] != have["ota"]:
            stop("refused", "FRL-E036", f"FRL_CC35_LAYOUT={said} contradicts what frl last programmed on this chip ({_layout_name(have)})",
                 "check what is on the board, then " + erase)
        if have != lay:
            stop("refused", "FRL-E036", f"the board holds {_layout_name(have)} and this image needs {_layout_name(lay)}; programming over it leaves the board silent",
                 erase)
        return "same as the last write"
    if not said:
        stop("refused", "FRL-E036", "frl has not programmed this chip before and cannot read its flash layout back",
             f"if the board runs a program built with {_layout_name(lay)} (TI's examples and uart2echo use two slots), run again with "
             f"FRL_CC35_LAYOUT={'ota' if lay['ota'] else 'single'}; if you do not know, " + erase)
    if STATED[said] != lay["ota"]:
        stop("refused", "FRL-E036", f"the board holds {'the two-slot OTA layout' if STATED[said] else 'the one-slot layout'} (FRL_CC35_LAYOUT={said}) and this image needs {_layout_name(lay)}",
             erase)
    return f"stated {said}"


def program(board, serial, settings):
    """Program the signed images one tool_settings.json names (a copy made by copy_bundle), then start them."""
    if Path(settings).name != "tool_settings.json":
        stop("refused", "FRL-E031", f"the Toolbox programs a tool_settings.json, not {Path(settings).name}", "this is a bug in the caller: report it")
    code, out = run(serial, ["programming", "--tool_settings", settings])
    if code is None:
        stop("failed", "FRL-E033", "the Toolbox stopped answering during programming; the board state is unknown",
             "program it again, or put a known image back with `frl restore`", toolbox=(out or "")[-600:])
    if code != 0 or not re.search(r"Writing binary size of \d+ bytes", out or ""):
        _failed(out, serial, "program")
    rc, rout = start(board, serial)          # the Toolbox leaves the chip waiting; a reset starts the new program
    if rc != 0:
        stop("unverified", "FRL-E042", f"the images were written, but the reset that starts them failed on probe {serial}: {(rout or '').strip()[-160:]}",
             "press S1 on the board (or unplug its USB-C for 5 s), then run `frl verify`", toolbox=(out or "")[-600:])
    return out


def start(board, serial):
    """Pulse the board's reset line through the probe (TI's xds110reset)."""
    x = toolfind.find("xdsdfu", required=False)
    exe = Path(x["exe"]).with_name("xds110reset") if x else None
    if not exe or not exe.is_file():
        stop("unverified", "FRL-E042", "xds110reset is missing, so the new program was not started", "press S1 on the board, then run `frl verify`")
    p = subprocess.run([str(exe), "-s", serial], capture_output=True, text=True, timeout=30)
    return p.returncode, p.stdout + p.stderr


def write_report(out):
    sizes = [int(n) for n in re.findall(r"Writing binary size of (\d+) bytes", out or "")]
    return {"write_s": None, "verify_s": None, "verify_path": "none: the Toolbox has no read-back", "words": [], "bytes": sum(sizes)}
