#!/usr/bin/env python3
"""Back up a board before a benchmark run, put it back after, and prove the read-back matches.

  python3 tools/benchmark/backup.py save <board> --probe S [--device D]       read every region twice, compare, keep
  python3 tools/benchmark/backup.py restore <backup folder> [--probe S]       write the restorable regions back
  python3 tools/benchmark/backup.py verify <backup folder> [--probe S]        read back now and compare with the backup

Regions come from tools/benchmark/recipes.json (boards.<board>.backup.regions); a region with
"restore": false is read and kept but never written (boot configuration). Reads and writes go through
TI's DSLite on one probe only, picked by serial number in a target configuration (an existing
<device>_<serial>.ccxml in a board folder, or one written from --device). Backups live in the per-user
data folder (macOS ~/Library/Application Support/FoxRedLabs/backups, Linux ~/.local/state/fox-red-labs/
backups, Windows %LOCALAPPDATA%\FoxRedLabs\backups; FRL_HOME moves it), outside any clone or worktree,
so deleting a checkout never deletes a backup. A board can hold someone else's firmware, so backups never
enter the repo or a share bundle. Older backups in tools/benchmark/local/backups/ still restore.

Whose board (2026-10-14): the probe serial is the one you select (--probe; the bench's own default comes
from the git-ignored settings.local.json "lab_probes", never from recipes.json). Where data/routes.json
knows how to read the chip's identity (TRACEID, DEVICEID, USERID on MSPM0), save records it and restore
and verify refuse a chip whose identity differs. backup.json is hashed into index.json beside the
backups at save time; an edited backup.json is refused.

Safety (Codex review 2026-10-08): restore and verify never trust backup.json on its own. Regions,
addresses, sizes, the restore flag, the probe and the tool script come from recipes.json for that board;
backup.json may only narrow (fewer regions, or restore switched off), never add a region or switch
restore on. Each region file's sha256 and size are checked before anything is written. The tool script
must be a file inside this repo (no absolute path, no '..'). The backup folder must sit under
tools/benchmark/local/backups/. Folders are created 0700 and files 0600: flash images can hold network keys.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common as c  # noqa: E402
import detect  # noqa: E402


sys.path.insert(0, str(c.ROOT / "tools" / "frl"))
from core import data_dir  # noqa: E402

LEGACY = c.LOCAL / "backups"
BACKUPS = Path(os.environ["FRL_BACKUPS"]) if os.environ.get("FRL_BACKUPS") else data_dir() / "backups"


def lab_probe(board):
    """The bench's own probe for a board, from the git-ignored settings file. None on anyone else's machine."""
    return (c.load_settings().get("lab_probes") or {}).get(board)


def index_path(root=None):
    return (root or BACKUPS) / "index.json"


def read_index(root=None):
    try:
        return json.loads(index_path(root).read_text())
    except (OSError, ValueError):
        return {}


def board_identity_spec(board):
    try:
        b = json.loads((c.ROOT / "data" / "routes.json").read_text())["boards"].get(board)
    except (OSError, ValueError):
        return None
    return (b or {}).get("target", {}).get("identity") and b


def private_umask():
    """Everything this tool creates (folders, images, logs) is readable by this user only."""
    os.umask(0o077)


def tool_script(script):
    """The tool script as a path inside the repo, or SystemExit. Absolute paths and '..' are refused."""
    if not isinstance(script, str) or not script or Path(script).is_absolute() or ".." in Path(script).parts:
        raise SystemExit(f"REFUSED: tool script {script!r} is absolute or climbs out with '..'")
    p = (c.ROOT / script).resolve()
    if not p.is_relative_to(c.ROOT.resolve()):
        raise SystemExit(f"REFUSED: tool script {script!r} resolves outside the repo")
    if not p.is_file():
        raise SystemExit(f"REFUSED: tool script {script!r} is not a file in this repo")
    return script


def check_folder(folder):
    """The backup folder as a resolved path under the backups folder (or the legacy one), or SystemExit."""
    raw = Path(folder)
    if raw.is_symlink():
        raise SystemExit(f"REFUSED: {folder} is a symlink")
    d = raw.resolve()
    roots = [BACKUPS.resolve(), LEGACY.resolve()]
    if not any(d != r and d.is_relative_to(r) for r in roots):
        raise SystemExit(f"REFUSED: {folder} is not a backup folder under {roots[0]}")
    if not (d / "backup.json").is_file():
        raise SystemExit(f"REFUSED: no backup.json in {d}")
    return d


def trusted_plan(d, recipes=None, serial=None, identity=None):
    """Read backup.json and reconcile it with recipes.json. Returns (man, regions, tool, ccxml).

    The memory policy (regions, restore flags, tool) comes from the board's recipe; whose board it is
    comes from the backup itself: the probe serial selected now must be the one it was read through, and
    the chip's identity read now must match the one recorded. backup.json must hash to its index entry.
    Each returned region is the backup.json entry with its address, size and name taken from the
    recipe, and 'restore' true only when both the recipe and backup.json say so."""
    raw = (d / "backup.json").read_bytes()
    man = json.loads(raw)
    boards = (recipes if recipes is not None else json.loads((c.HERE / "recipes.json").read_text()))["boards"]
    rec = boards.get(man.get("board"))
    if not rec or not (rec.get("backup") or {}).get("regions"):
        raise SystemExit(f"REFUSED: recipes.json has no backup recipe for board {man.get('board')!r}")
    legacy = d.resolve().is_relative_to(LEGACY.resolve())
    if legacy:
        if man.get("probe") not in (rec.get("probe"), lab_probe(man.get("board"))):
            raise SystemExit(f"REFUSED: backup.json probe {man.get('probe')!r} is not the bench probe for this older backup")
    else:
        want = read_index(d.parent).get(d.name)
        if not want:
            raise SystemExit(f"REFUSED: {d.name} is not in {index_path(d.parent)}; it was not made by backup.py save here")
        if want != sha(raw):
            raise SystemExit("REFUSED: backup.json changed since it was saved (its sha256 no longer matches index.json)")
    if not c.SAFE_SERIAL.fullmatch(str(man.get("probe") or "")):
        raise SystemExit(f"REFUSED: backup.json probe {man.get('probe')!r} is not a probe serial")
    if serial is not None and serial != man.get("probe"):
        raise SystemExit(f"REFUSED: this backup was read through probe {man.get('probe')}, not {serial}; it belongs to another board")
    if man.get("identity") and identity is not None:
        for k in ("traceid", "deviceid"):
            if man["identity"].get(k) != identity.get(k):
                raise SystemExit(f"REFUSED: the chip on probe {serial} has {k} {identity.get(k)}, the backup was read from {man['identity'].get(k)}; it belongs to another board")
    if man.get("identity") and identity is None and board_identity_spec(man.get("board")):
        raise SystemExit("REFUSED: this backup records the chip's identity; read it from the board before writing (use --probe)")
    rtool = (rec["backup"].get("tool"))
    mtool = man.get("tool")
    if (rtool or {}).get("kind") != (mtool or {}).get("kind") or (rtool or {}).get("script") != (mtool or {}).get("script"):
        raise SystemExit("REFUSED: backup.json names a different tool than recipes.json for this board")
    if rtool:
        tool_script(rtool.get("script"))
    by_key = {(int(r["addr"], 16), int(r["bytes"])): r for r in rec["backup"]["regions"]}
    seen, regions = set(), []
    for r in man.get("regions", []):
        try:
            key = (int(r["addr"], 16), int(r["bytes"]))
        except (KeyError, TypeError, ValueError):
            raise SystemExit(f"REFUSED: region {r!r} has no usable addr and bytes")
        want = by_key.get(key)
        if not want:
            raise SystemExit(f"REFUSED: backup.json region {r.get('name')!r} at {r.get('addr')} for {r.get('bytes')} bytes is not in recipes.json")
        if key in seen:
            raise SystemExit(f"REFUSED: backup.json lists region {r.get('name')!r} twice")
        seen.add(key)
        if Path(str(r.get("file"))).name != r.get("file"):
            raise SystemExit(f"REFUSED: backup.json names {r.get('file')!r}: not a plain file name")
        out = dict(r)
        out["name"], out["addr"], out["bytes"] = want["name"], want["addr"], int(want["bytes"])
        out["restore"] = bool(want.get("restore", True)) and bool(r.get("restore", True))
        regions.append(out)
    if not regions:
        raise SystemExit("REFUSED: backup.json lists no regions")
    ccxml = None if rtool else ccxml_for(man.get("probe"), rec.get("device"))
    return man, regions, rtool, ccxml


def check_file_hash(d, r):
    """The region file exists, is the recipe's size, and its sha256 matches backup.json, or SystemExit."""
    f = d / r["file"]
    if f.is_symlink() or not f.is_file():
        raise SystemExit(f"REFUSED: {r['file']} is missing or not a plain file")
    data = f.read_bytes()
    if len(data) != r["bytes"]:
        raise SystemExit(f"REFUSED: {r['file']} is {len(data)} bytes, the recipe says {r['bytes']}")
    if sha(data) != r.get("sha256"):
        raise SystemExit(f"REFUSED: {r['file']} sha256 {sha(data)[:16]} does not match backup.json {str(r.get('sha256'))[:16]}; the file changed since it was saved")


def ccxml_for(serial, device):
    """The board folder's own target configuration for this probe, else one written from the device name."""
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    p = detect.find_ccxml(serial)
    if p and (not device or p.stem.lower().startswith(device.lower())):
        return p
    if not device:
        raise SystemExit(f"no *_{serial}.ccxml in a board folder: pass --device <targetdb device name>")
    return detect.make_ccxml(serial, device)


# A whole-flash write through OpenOCD is slow on some parts: the LP-MSPM33C321A's main-flash restore took 232 s alone
# and passed 300 s with six boards writing at once (2026-10-25). Reads and writes get the same, generous limit.
OCD_TIMEOUT = 900


def ocd(script, serial, *cmds, route=None):
    """Run OpenOCD commands on this probe serial and no other. A board with a guided route (data/routes.json,
    `route` names it) runs OpenOCD straight from Python with the OpenOCD frl found, so the backup works on
    macOS, Linux and Windows alike; the rest go through the board's own script, with the serial passed
    explicitly, replacing any PROBE the caller's environment carries."""
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    if route:
        import target
        from core import board_route
        from lock import ProbeLock
        with ProbeLock(serial, "", "backup.py"):
            code, out = target.run(board_route(route), serial, list(cmds), timeout=OCD_TIMEOUT)
        if code is None:
            raise RuntimeError(f"OpenOCD gave no answer within {OCD_TIMEOUT} s, so the board state is unknown; "
                               "run the same restore again (it checks the board before it writes)")
        if code != 0:
            raise RuntimeError(((out or "").strip()[-400:]) or f"OpenOCD exit {code}")
        return out
    env = dict(os.environ, PROBE=serial)
    r = subprocess.run([str(c.ROOT / tool_script(script)), *cmds], capture_output=True, text=True, timeout=OCD_TIMEOUT, env=env)
    if r.returncode != 0:
        raise RuntimeError((r.stdout + r.stderr).strip()[-400:])
    return r.stdout + r.stderr


def _route(tool):
    import sys as _s
    _s.path.insert(0, str(c.ROOT / "tools" / "frl"))
    from core import board_route
    return board_route(tool["route"])


def read_region(dsl, ccxml, addr, nbytes, out, tool=None, serial=None):
    if tool and tool.get("kind") == "ccs13":        # CC13x4, CC26x4 through CCStudio's Python scripting
        import ccs13
        from lock import ProbeLock
        if out.exists():
            out.unlink()
        with ProbeLock(serial, "", "backup.py"):
            ccs13.backup_region(_route(tool), serial, addr, nbytes, str(out))
        data = out.read_bytes()
        if len(data) != nbytes:
            raise RuntimeError(f"CCStudio saved {len(data)} bytes, wanted {nbytes}")
        return data
    if tool and tool.get("kind") == "dss":          # C2000 through CCStudio scripting: word addresses, so half the bytes
        import dss
        from lock import ProbeLock
        if out.exists():
            out.unlink()
        with ProbeLock(serial, "", "backup.py"):
            dss.backup_region(_route(tool), serial, addr, nbytes // 2, str(out))
        data = out.read_bytes()
        if len(data) != nbytes:
            raise RuntimeError(f"CCStudio saved {len(data)} bytes, wanted {nbytes}")
        return data
    if tool and tool.get("kind") == "openocd":
        if out.exists():
            out.unlink()
        ocd(tool["script"], serial, "halt", f"dump_image {{{out}}} 0x{addr:08X} 0x{nbytes:X}", "resume", route=tool.get("route"))
        data = out.read_bytes()
        if len(data) != nbytes:
            raise RuntimeError(f"OpenOCD dumped {len(data)} bytes, wanted {nbytes}")
        return data
    words = detect.dslite_read(dsl, ccxml, addr, nbytes // 4, out.with_suffix(".dslite.txt"))
    if len(words) < nbytes // 4:
        raise RuntimeError(f"DSLite returned {len(words)} words, wanted {nbytes // 4}")
    data = b"".join(w.to_bytes(4, "little") for w in words[: nbytes // 4])
    out.write_bytes(data)
    return data


def sha(b):
    return hashlib.sha256(b).hexdigest()


def read_identity(board, serial):
    """The chip's identity through the board's route (data/routes.json), or None where none is known."""
    b = board_identity_spec(board)
    if not b:
        return None
    import target  # tools/frl
    return target.identity(b, serial)


def save(board, serial, identity=None, device=None):
    """Read every region twice, compare, keep. Returns the backup folder."""
    c.need(c.SAFE_ID, board, "board")
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    rec = json.loads((c.HERE / "recipes.json").read_text())["boards"].get(board, {})
    device = device or rec.get("device")
    regions = (rec.get("backup") or {}).get("regions")
    if not regions:
        raise SystemExit(f"{board}: no backup regions in tools/benchmark/recipes.json; add them first")
    tool = (rec.get("backup") or {}).get("tool")
    if tool:
        tool_script(tool.get("script"))
    dsl = None if tool else c.find_dslite(c.find_ccs())
    if not tool and not dsl:
        raise SystemExit("DSLite not found (run doctor.py)")
    need = sum(r["bytes"] for r in regions) * 6 / 1024 ** 3
    if c.disk_free_gb() < need + 0.2:
        raise SystemExit(f"not enough free disk for the backup ({c.disk_free_gb():.2f} GB free)")
    ccxml = None if tool else ccxml_for(serial, device)
    BACKUPS.mkdir(parents=True, exist_ok=True)
    os.chmod(BACKUPS, 0o700)
    d = BACKUPS / f"{board}-{serial}-{dt.datetime.now():%Y%m%d-%H%M%S}"
    d.mkdir(mode=0o700)
    os.chmod(d, 0o700)
    man = {"board": board, "probe": serial, "identity": identity, "ccxml": str(ccxml) if ccxml else None, "tool": tool,
           "made": dt.datetime.now().isoformat(timespec="seconds"), "regions": []}
    for r in regions:
        addr, n = int(r["addr"], 16), int(r["bytes"])
        name = r["name"].split(" (")[0].replace(" ", "-")
        one = read_region(dsl, ccxml, addr, n, d / f"{name}.bin", tool, serial)
        two = read_region(dsl, ccxml, addr, n, d / f"{name}.second.bin", tool, serial)
        if one != two:
            raise SystemExit(f"{r['name']}: the two reads differ; do not run anything until this is understood")
        (d / f"{name}.second.bin").unlink()
        man["regions"].append({"name": r["name"], "file": f"{name}.bin", "addr": r["addr"], "bytes": n,
                               "sha256": sha(one), "restore": r.get("restore", True)})
        print(f"{r['name']}: {n} bytes, read twice, identical, sha256 {sha(one)[:16]}")
    raw = (json.dumps(man, indent=1) + "\n").encode()
    (d / "backup.json").write_bytes(raw)
    for f in d.iterdir():
        os.chmod(f, 0o600)
    idx = read_index()
    idx[d.name] = sha(raw)
    tmp = index_path().with_suffix(".tmp")
    tmp.write_text(json.dumps(idx, indent=1) + "\n")
    os.chmod(tmp, 0o600)
    os.replace(tmp, index_path())
    print(f"Backup: {d}")
    return d


def restore(folder, serial, identity=None):
    """Write the restorable regions back to the board this backup came from, then read back and compare."""
    d = check_folder(folder)
    man, regions, tool, ccxml = trusted_plan(d, serial=serial, identity=identity)
    blocked = ((board_identity_spec(man.get("board")) or {}).get("target") or {}).get("write_blocked")
    if blocked:
        raise SystemExit(f"REFUSED: the route for {man.get('board')} does not write the board: {blocked}")
    for r in regions:           # nothing is written until every file to be written has passed
        if r["restore"]:
            check_file_hash(d, r)
    dsl = None if tool else c.find_dslite(c.find_ccs())
    log = []
    if tool and tool.get("restore") == "dslite_lpf3":     # CC23xx, CC27xx: every region in one chip-erase load
        import lpf3
        from lock import ProbeLock
        parts = [(int(r["addr"], 16), (d / r["file"]).read_bytes()) for r in regions if r["restore"]]
        with ProbeLock(serial, "", "backup.py"):
            out = lpf3.restore(_route(tool), serial, parts, d)
        (d / "restore-dslite.log").write_text(out)
        log.append(f"{', '.join(r['name'] for r in regions if r['restore'])}: DSLite chip erase and load done")
        print(log[-1])
        regions = []
    for r in regions:
        if not r["restore"]:
            continue
        if tool and tool.get("kind") == "ccs13":
            import ccs13
            from lock import ProbeLock
            with ProbeLock(serial, "", "backup.py"):
                ccs13.restore_region(_route(tool), serial, str(d / r["file"]), int(r["addr"], 16))
            log.append(f"{r['name']}: CCStudio load and verify done")
            print(log[-1])
            continue
        if tool and tool.get("kind") == "dss":
            import dss
            from lock import ProbeLock
            with ProbeLock(serial, "", "backup.py"):
                dss.restore_region(_route(tool), serial, str(d / r["file"]), int(r["addr"], 16), int(r["bytes"]) // 2,
                                   str(d / f"restore-readback-{r['file']}"))
            log.append(f"{r['name']}: CCStudio program, verify and read back done")
            print(log[-1])
            continue
        if tool and tool.get("kind") == "openocd":
            out = ocd(tool["script"], serial, "reset halt", f"program {{{d / r['file']}}} {r['addr']} verify", "reset run",
                      route=tool.get("route"))
            (d / f"restore-{r['file']}.log").write_text(out)
            log.append(f"{r['name']}: OpenOCD program and verify done")
            print(log[-1])
            continue
        r2 = subprocess.run([str(dsl), "flash", "-c", str(ccxml), "-e", "-f", "-v", f"{d / r['file']},{r['addr']}"],
                            capture_output=True, text=True, timeout=300)
        (d / f"restore-{r['file']}.log").write_text(r2.stdout + r2.stderr)
        log.append(f"{r['name']}: DSLite flash exit {r2.returncode}")
        print(log[-1])
        if r2.returncode != 0:
            raise SystemExit((r2.stdout + r2.stderr)[-600:])
    if not verify(folder, serial, identity):
        raise SystemExit("REFUSED: the read-back after restore does not match the backup")
    if tool and tool.get("restore") == "dslite_lpf3":     # the read-back left the part halted in its boot ROM
        import lpf3
        lpf3.start(_route(tool), serial)
        log.append("started with DSLite's device reset")
    return log


def verify(folder, serial, identity=None):
    d = check_folder(folder)
    man, regions, tool, ccxml = trusted_plan(d, serial=serial, identity=identity)
    for r in regions:
        check_file_hash(d, r)
    dsl = None if tool else c.find_dslite(c.find_ccs())
    ok = True
    for r in regions:
        now = read_region(dsl, ccxml, int(r["addr"], 16), r["bytes"], d / f"readback-{r['file']}", tool, serial)
        same = sha(now) == r["sha256"]
        ok &= same
        print(f"{r['name']}: read back sha256 {sha(now)[:16]}, backup {r['sha256'][:16]}: {'MATCH' if same else 'DIFFERENT'}")
    (d / "verify.json").write_text(json.dumps({"when": dt.datetime.now().isoformat(timespec='seconds'), "match": ok}) + "\n")
    return ok


def cli_serial(a, board=None, man=None):
    """--probe, else the probe this backup was read through, else the bench's own default. Must be on USB."""
    serial = a.probe or (man or {}).get("probe") or (lab_probe(board) if board else None)
    if not serial:
        raise SystemExit("pass --probe <serial> (python3 tools/frl/frl.py detect lists the probes on USB)")
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    return serial


def cmd_save(a):
    serial = cli_serial(a, a.board)
    save(a.board, serial, identity=read_identity(a.board, serial), device=a.device)


def cmd_restore(a):
    d = check_folder(a.folder)
    man = json.loads((d / "backup.json").read_text())
    serial = cli_serial(a, man.get("board"), man)
    restore(a.folder, serial, read_identity(man.get("board"), serial))


def cmd_verify(a):
    d = check_folder(a.folder)
    man = json.loads((d / "backup.json").read_text())
    serial = cli_serial(a, man.get("board"), man)
    sys.exit(0 if verify(a.folder, serial, read_identity(man.get("board"), serial)) else 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("save")
    p.add_argument("board")
    p.add_argument("--probe")
    p.add_argument("--device")
    for name in ("restore", "verify"):
        p = sub.add_parser(name)
        p.add_argument("folder")
        p.add_argument("--probe")
    a = ap.parse_args()
    private_umask()
    {"save": cmd_save, "restore": cmd_restore, "verify": cmd_verify}[a.cmd](a)


if __name__ == "__main__":
    main()
