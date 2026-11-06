#!/usr/bin/env python3
"""Flash a prebuilt TI demo by name, with no build. Dry run unless you pass --yes.

  python3 tools/demo_flash.py list [board]          the menu of prebuilt hex files per board
  python3 tools/demo_flash.py flash <board> <name>  print the UniFlash command (dry run)
  python3 tools/demo_flash.py flash <board> <name> --yes
                                                    back up the board, flash, print the restore command

<board> is a key in tools/board_sweep/boards.json (LP_EM_CC2340R5, LP_EM_CC1354P10_1, ...).
<name> is the short name from `list` (the file name without .hex; add the stack folder if two share a name).

Hex files come from the installed TI SDKs under /Applications/ti: prebuilt_hexfiles and hexfiles folders
inside examples/, matched to a board by its name in the path.

Safety, same as tools/board_sweep/ccs_run_session.py:
  - The backup comes first: flash and the configuration area, read with DSLite. DSLite takes the length of
    `memory --range` in 32-bit words, not bytes, so the length is divided by 4 and the file size is checked.
    A blank backup, a wrong-size backup or a failed read stops the run before anything is written.
  - The backup goes outside the repo (it can hold network keys): out/demo-flash/<board>/<time>/.
  - Every path given to DSLite is absolute.
  - Boot managers and over-the-air update images (mcuboot, oad, bim) refuse unless you add --allow-boot.
  - Boards that flash with OpenOCD (the MSPM0 boards) are listed but not flashed here.
  - The restore is not automatic: the script prints the exact command to put the backup back.
"""
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARDS = ROOT / "tools" / "board_sweep" / "boards.json"
DSLITE_SH = "/Applications/ti/uniflash_9.6.0/dslite.sh"
TI = Path("/Applications/ti")
BOOT = re.compile(r"mcuboot|oad|bim|boot_manager|bsl|nonmain|ccfg", re.I)


def load_boards():
    return {k: v for k, v in json.load(open(BOARDS)).items() if not k.startswith("_")}


def find_hex(board_key, board):
    """Prebuilt hex files for one board: list of dicts (name, path, size, stack, boot)."""
    want = board.get("sdk_board", board_key)
    found = []
    for pat in ("*sdk*/examples/**/prebuilt_hexfiles/**/*.hex", "*/examples/**/*.hex"):
        for f in TI.glob(pat):
            if want in f.parts or any(p == want for p in f.parts):
                found.append(f)
    out, seen = [], set()
    for f in sorted(set(found)):
        parts = f.parts
        i = parts.index(want)
        stack = parts[i + 1] if i + 1 < len(parts) - 1 else ""
        sdk = next((p for p in parts if p.startswith(("simplelink", "mspm", "c2000", "am13"))), "")
        out.append({"name": f.stem, "path": f, "size": f.stat().st_size, "stack": stack, "sdk": sdk,
                    "boot": bool(BOOT.search(f.name))})
    # Names that two files share get the stack folder in front.
    count = {}
    for e in out:
        count[e["name"]] = count.get(e["name"], 0) + 1
    for e in out:
        e["key"] = e["name"] if count[e["name"]] == 1 else f"{e['stack']}/{e['name']}"
    return out


def cmd_list(boards, only=None):
    for key, b in boards.items():
        if only and key != only:
            continue
        hexes = find_hex(key, b)
        how = "UniFlash (dslite)" if b.get("ccxml") else "OpenOCD (not flashed by this tool)"
        print(f"\n{key}  probe {b.get('probe')}  {how}")
        if not hexes:
            print("  no prebuilt hex files found in the installed SDKs")
            continue
        w = max(len(e["key"]) for e in hexes) + 2
        print(f"  {'name':<{w}}{'stack':<12}{'hex kB':>8}  note")
        for e in hexes:
            note = "boot or update image: needs --allow-boot" if e["boot"] else ""
            print(f"  {e['key']:<{w}}{e['stack']:<12}{e['size'] / 1024:>8.0f}  {note}")
    if only and only not in boards:
        sys.exit(f"unknown board {only}; boards: {', '.join(boards)}")


def sh(args, timeout=240):
    p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def read_mem(dslite, ccxml, start, length_bytes, out):
    # DSLite takes the length in 32-bit words, not bytes
    words = length_bytes // 4
    rc, log = sh([dslite, "--mode", "memory", "-c", str(ccxml), "--range", f"{hex(start)},{hex(words)}", "-o", str(out), "-t", "120"])
    if rc or not out.exists() or out.stat().st_size != length_bytes:
        size = out.stat().st_size if out.exists() else "no file"
        sys.exit(f"REFUSED: memory read at {hex(start)} failed (wanted {length_bytes} bytes, got {size}). "
                 f"Nothing was flashed.\n{log[-400:]}")
    return out.read_bytes()


def ihex(chunks):
    def rec(a16, typ, data):
        b = bytes([len(data), a16 >> 8, a16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()
    lines, upper = [], None
    for base, data in chunks:
        for off in range(0, len(data), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16
                lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, data[off:off + 16]))
    return "\n".join(lines + [":00000001FF"]) + "\n"


def cmd_flash(boards, key, name, yes, allow_boot):
    if key not in boards:
        sys.exit(f"unknown board {key}; boards: {', '.join(boards)}")
    b = boards[key]
    if not b.get("ccxml"):
        sys.exit(f"REFUSED: {key} flashes with OpenOCD, not UniFlash. This tool does not flash it. "
                 f"See mspm0g3507/example-sweep/run_one.py.")
    hexes = find_hex(key, b)
    hits = [e for e in hexes if name in (e["key"], e["name"])]
    if not hits:
        sys.exit(f"no demo named {name!r} for {key}. Run: python3 tools/demo_flash.py list {key}")
    if len(hits) > 1:
        sys.exit(f"{name!r} matches more than one file; use one of: {', '.join(e['key'] for e in hits)}")
    e = hits[0]
    if e["boot"] and not allow_boot:
        sys.exit(f"REFUSED: {e['name']} is a boot manager or update image. It can replace the part of flash "
                 f"that decides what boots. Add --allow-boot if you mean it.")
    ccxml = (ROOT / b["ccxml"]).resolve()
    hexpath = e["path"].resolve()
    cmd = [DSLITE_SH, "--mode", "flash", "-c", str(ccxml), "-e", "-f", "-u", str(hexpath)]
    print(f"board   {key} (probe {b['probe']})\ndemo    {e['key']}  {e['size'] / 1024:.0f} kB hex from {e['sdk']}")
    print("backup  would read:", ", ".join(f"{hex(s)} for {n} bytes" for s, n in b["backup"]))
    print("command", " ".join(cmd))
    if b.get("start") == "ccs_session":
        print("note    this board's notes say only one DSLite session works per power-up while a program runs "
              "(error -242). Power-cycle the board before the backup, and again before the flash if it fails.")
    if not yes:
        print("\nDRY RUN: nothing was read or written. Add --yes to back up, flash and print the restore command.")
        return
    bdir = Path.cwd() / "out/demo-flash" / key / time.strftime("%Y%m%d-%H%M%S")
    os.umask(0o077)                      # backups can hold network keys: this user only
    bdir.mkdir(parents=True, exist_ok=True); os.chmod(bdir, 0o700)   # a fresh timestamped folder
    chunks = []
    for start, length in b["backup"]:
        f = bdir / f"backup_{start:#x}.bin"
        chunks.append((start, read_mem(DSLITE_SH, ccxml, start, length, f)))
    if chunks[0][1][:8] in (b"\xff" * 8, b"\x00" * 8):
        sys.exit("REFUSED: backup looks blank. Nothing was flashed.")
    restore = bdir / "restore.hex"
    restore.write_text(ihex(chunks))
    print(f"backup ok in {bdir}")
    t0 = time.time()
    rc, log = sh(cmd, timeout=300)
    print(log[-600:])
    print(f"flash {'ok' if rc == 0 else 'FAILED'} in {time.time() - t0:.1f} s")
    print("to put the board back:\n  " + " ".join([DSLITE_SH, "--mode", "flash", "-c", str(ccxml), "-e", "-f", "-u", str(restore.resolve())]))
    if rc:
        sys.exit(1)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if not args or args[0] not in ("list", "flash") or (args[0] == "flash" and len(args) != 3):
        print(__doc__)
        sys.exit(2)
    boards = load_boards()
    if args[0] == "list":
        cmd_list(boards, args[1] if len(args) > 1 else None)
    else:
        cmd_flash(boards, args[1], args[2], "--yes" in flags, "--allow-boot" in flags)


if __name__ == "__main__":
    main()
