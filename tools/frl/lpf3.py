"""The CC23xx and CC27xx write path: TI's DSLite, the way TI's own tools write these parts.

DSLite offers three erase choices for them (FlashEraseSetting): Chip Erase, Chip Erase keeping the sectors the
CCFG lists, or no erase. None writes a single sector, so a write erases main flash and the CCFG (the boot
configuration at 0x4E020000) and then writes the image, whose CCFG SysConfig generated. Before that, frl has
saved main flash and the CCFG (read twice), and the restore writes both back in one load. target.py and backup.py
call these when a route says "write": "dslite_lpf3".

Reading memory halts these parts, so the program's own check is a serial message, not an LED pin read.
"""
import struct
import subprocess
import tempfile
import time
from pathlib import Path

import toolfind
from core import hexint, stop

HERE = Path(__file__).resolve().parent
ERASE_WARNING = ("this chip's TI tools erase the whole chip before writing, including its boot configuration (CCFG); "
                 "the backup frl took first puts both back")


def ccxml(board, serial):
    """The route's CCStudio connection file with this probe's serial."""
    d = Path(tempfile.mkdtemp(prefix="frl-lpf3-"))
    p = d / "target.ccxml"
    p.write_text((HERE / "ccxml" / board["target"]["ccxml"]).read_text(encoding="utf-8").replace("{SERIAL}", serial),
                 encoding="utf-8")
    return p


def dslite(args, timeout=300):
    """(exit, output) of one DSLite call. A flash call passes -e (verbose, not erase), the only way it prints "Success"."""
    exe = toolfind.find("dslite")["exe"]
    try:
        r = subprocess.run([exe] + [str(a) for a in args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                           timeout=timeout)
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or "") if isinstance(e.stdout, str) else ""
    return r.returncode, r.stdout


def _failed(out, serial, what):
    if "in use" in out.lower() or "busy" in out.lower():
        stop("failed", "FRL-E004", f"probe {serial} could not be opened; another program may hold it",
             "close CCStudio, UniFlash or another debugger, then try again", dslite=out[-600:])
    stop("failed", "FRL-E033" if what == "write" else "FRL-E006", f"DSLite could not {what} the board on probe {serial}",
         "run `frl verify-image` before anything else; the backup puts the chip back with `frl restore`", dslite=out[-800:])


def program(board, serial, image):
    """Chip erase and write one image (TI's default erase), with DSLite's own fast verification."""
    need_scfg = [w for w in board["target"].get("writable", []) if w["name"].startswith("SCFG")]
    if need_scfg:     # CC27xx: the chip erase clears the SCFG, and with no valid SCFG the debug port stays closed
        lo, hi = hexint(need_scfg[0]["start"]), hexint(need_scfg[0]["end"])
        if not any(lo <= a < hi for a, _ in elf_loads(image)):
            stop("failed", "FRL-E036", "this image carries no SCFG, and the chip erase would leave the board's debug port closed",
                 "build the image with TI Arm Clang from the SDK example, whose output includes the CCFG and SCFG")
    code, out = dslite(["flash", "-c", ccxml(board, serial)] + erase_args(board)
                       + ["-s", "VerifyAfterProgramLoad=Fast verification", "-r", "0", "-e", "-f", image], timeout=300)
    if code != 0 or "Success" not in (out or ""):
        _failed(out or "", serial, "write")
    return out


def erase_args(board):
    """DSLite's erase choice for this part. The CC23xx loader offers "FlashEraseSetting"; the CC27xx loader does not
    (DSLite answers "Unknown Property: FlashEraseSetting", CC2755P10, 2026-11-02) and instead erases by default,
    keeping the HSM sectors (RetainMainSectors). A route lists the settings it wants in target.dslite_erase_settings."""
    settings = board["target"].get("dslite_erase_settings", ["FlashEraseSetting=Chip Erase"])
    return [x for st in settings for x in ("-s", st)]


def start(board, serial):
    """Start the program: DSLite's "Device reset (automatic connect/disconnect)". TI's OpenOCD halts these parts in
    the boot ROM on every connect and its `reset run` leaves them there (LP-EM-CC2340R5, 2026-10-24)."""
    code, out = dslite(["flash", "-c", ccxml(board, serial), "-r", "0", "-e"], timeout=120)
    if code != 0 or "Success" not in (out or ""):
        _failed(out or "", serial, "reset")
    return out


def sample(board, serial, led, seconds, max_reads=16):
    """The pin check for these parts. TI's OpenOCD parks the core in the boot ROM on every attach, so a pin read through
    it shows the output disabled whatever the program does (LP-EM-CC2340R5, 2026-11-02). So: a real reset through
    DSLite, then DSLite reads (it attaches without a reset) of the pin's DOUT and DOE words. Each read takes about
    4 s, far too slow to time a pattern, so the sample says "timed": False and holds the levels seen."""
    base, off, bit = hexint(led["gpio_base"]), board["gpio_offsets"], int(led["bit"])
    dout, doe = base + hexint(off["dout"]), base + hexint(off["doe"])
    start(board, serial)
    tmp = Path(tempfile.mkdtemp(prefix="frl-lpf3-s-"))
    levels, enabled, t0 = [], None, time.time()
    for k in range(max_reads):
        if k:     # a read takes about 4 s, close to three periods of the default pattern, so back-to-back reads keep landing
            time.sleep(((k * 0.618) % 1) * 1.4)     # on the same phase of it (CC2755P10 run 3, 2026-11-02: 8 reads, all "off")
        blob = read(board, serial, dout, doe - dout + 4, tmp / f"pin{k}.bin")
        enabled = (struct.unpack_from("<I", blob, doe - dout)[0] >> bit) & 1
        levels.append((struct.unpack_from("<I", blob, 0)[0] >> bit) & 1)
        if enabled and len(set(levels)) == 2 and time.time() - t0 >= seconds:
            break
        if not enabled:
            break
    ms = round((time.time() - t0) * 1000)
    return {"transitions": [], "samples": len(levels), "span_ms": ms, "output_enabled": enabled, "timed": False,
            "levels_seen": sorted(set(levels))}


def elf_loads(path):
    """[(address, bytes)] for the loaded segments of a 32-bit little-endian Arm ELF image."""
    data = Path(path).read_bytes()
    phoff, = struct.unpack_from("<I", data, 28)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    out = []
    for k in range(phnum):
        p_type, p_offset, _, p_paddr, p_filesz, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + k * phentsize)
        if p_type == 1 and p_filesz:
            out.append((p_paddr, data[p_offset:p_offset + p_filesz]))
    return out


def read(board, serial, addr, nbytes, out):
    """nbytes from addr through DSLite (its length counts 32-bit words), as bytes."""
    words = (nbytes + 3) // 4
    code, log = dslite(["memory", "-c", ccxml(board, serial), "-r", f"0x{addr:08X},{words}", "-o", out], timeout=300)
    if code != 0 or not Path(out).is_file():
        _failed(log or "", serial, "read")
    return Path(out).read_bytes()[:nbytes]


def verify_image(board, serial, image):
    """(same, report): main flash holds the image's main-flash bytes. The CCFG segment is not compared: TI's flash
    loader fills in the CCFG's CRC fields as it writes, so the image's copy differs from the chip's by design."""
    main = [(hexint(w["start"]), hexint(w["end"])) for w in board["target"]["writable"] if "main" in w["name"]]
    tmp = Path(tempfile.mkdtemp(prefix="frl-lpf3-v-"))
    report, same = [], True
    for addr, blob in elf_loads(image):
        if not any(s <= addr and addr + len(blob) <= e for s, e in main):
            report.append(f"0x{addr:08X} {len(blob)} B: not compared (outside main flash)")
            continue
        got = read(board, serial, addr, len(blob), tmp / f"seg-{addr:08X}.bin")
        ok = got == blob
        same &= ok
        report.append(f"0x{addr:08X} {len(blob)} B: {'match' if ok else 'DIFFERENT'}")
    return same, "\n".join(report) + "\nverified" if same else "\n".join(report) + "\ncontents differ"


def ihex(regions):
    """Intel HEX text for [(address, bytes)], 16 bytes a record, with extended linear address records."""
    lines, upper = [], None

    def rec(kind, addr, payload):
        body = bytes([len(payload), (addr >> 8) & 0xFF, addr & 0xFF, kind]) + payload
        return ":" + (body + bytes([(-sum(body)) & 0xFF])).hex().upper()
    for base, blob in sorted(regions):
        for off in range(0, len(blob), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16
                lines.append(rec(4, 0, upper.to_bytes(2, "big")))
            lines.append(rec(0, a & 0xFFFF, blob[off:off + 16]))
    lines.append(":00000001FF")
    return "\n".join(lines) + "\n"


def restore(board, serial, regions, workdir):
    """Write every saved region back in one DSLite load (one chip erase), from [(address, bytes)]."""
    hexfile = Path(workdir) / "restore.hex"
    hexfile.write_text(ihex(regions), encoding="ascii")
    code, out = dslite(["flash", "-c", ccxml(board, serial)] + erase_args(board)
                       + ["-s", "VerifyAfterProgramLoad=Fast verification", "-r", "0", "-e", "-f", hexfile], timeout=600)
    if code != 0 or "Success" not in (out or ""):
        _failed(out or "", serial, "write")
    return out
