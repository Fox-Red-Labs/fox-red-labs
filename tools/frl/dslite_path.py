"""The second write path: TI's DSLite (inside UniFlash and CCStudio), for a board whose first path is OpenOCD or
CCStudio scripting. frl takes it when the first path's tool is not installed, or when FRL_WRITE_PATH=dslite asks
for it, and the result names the path used.

Each family gets the least erasing setting DSLite offers (the route's "dslite_erase"):
  MSPM0, MSPM33  FlashEraseSelection "Erase MAIN memory sectors by range": only the sectors the image covers
  AM13           "Necessary Bank(s) Only (for Program Load)"
  C2000          "Necessary Sectors Only (for Program Load)"
NONMAIN, the CCFG and every boot configuration area stay out of reach: an image that loads there is refused before
DSLite runs (target.check_writes).
"""
from pathlib import Path
import tempfile

import lpf3

HERE = Path(__file__).resolve().parent


def available(board):
    return bool(board["target"].get("dslite_ccxml"))


def ccxml(board, serial):
    d = Path(tempfile.mkdtemp(prefix="frl-dslite-"))
    p = d / "target.ccxml"
    p.write_text((HERE / "ccxml" / board["target"]["dslite_ccxml"]).read_text(encoding="utf-8").replace("{SERIAL}", serial),
                 encoding="utf-8")
    return p


def erase_settings(board, image):
    """DSLite -s settings for this route and image. "{range}" becomes the start and end of the image's bytes in main
    flash, which DSLite widens to whole sectors, counted from the route's dslite_range_base."""
    out = []
    for s in board["target"].get("dslite_erase", []):
        if s == "{range}":
            segs = [(a, len(b)) for a, b in lpf3.elf_loads(image)]
            base = int(str(board["target"].get("dslite_range_base", "0")), 0)   # MSPM33: image at 0x10000000, DSLite counts from 0
            lo, hi = min(a for a, _ in segs) - base, max(a + n for a, n in segs) - 1 - base
            out += [f"FlashEraseSectorsStartAddress={lo}", f"FlashEraseSectorsEndAddress={hi}"]
        else:
            out.append(s)
    return out


def program(board, serial, image):
    """Write one image through DSLite with the route's erase setting, its fast verification, then run."""
    args = ["flash", "-c", ccxml(board, serial)]
    for s in erase_settings(board, image) + ["VerifyAfterProgramLoad=Fast verification"]:
        args += ["-s", s]
    code, out = lpf3.dslite(args + ["-u", "-e", "-f", image], timeout=300)
    if code != 0 or "Success" not in (out or ""):
        lpf3._failed(out or "", serial, "write")
    return out


def verify_image(board, serial, image):
    """(same, report): the image's loaded bytes are in flash, read back through DSLite. Byte-addressed parts only;
    a C2000 counts 16-bit words, so its check is DSLite's own verification during the write."""
    if board["target"].get("word_addressed"):
        return True, "checked by DSLite's verification during the write\nverified"
    tmp = Path(tempfile.mkdtemp(prefix="frl-dslite-v-"))
    report, same = [], True
    for addr, blob in lpf3.elf_loads(image):
        words = (len(blob) + 3) // 4
        out = tmp / f"seg-{addr:08X}.bin"
        code, log = lpf3.dslite(["memory", "-c", ccxml(board, serial), "-r", f"0x{addr:08X},{words}", "-o", out])
        if code != 0 or not out.is_file():
            lpf3._failed(log or "", serial, "read")
        ok = out.read_bytes()[:len(blob)] == blob
        same &= ok
        report.append(f"0x{addr:08X} {len(blob)} B: {'match' if ok else 'DIFFERENT'}")
    return same, "\n".join(report) + ("\nverified" if same else "\ncontents differ")


def chosen(board):
    """True when this write goes through DSLite: asked for with FRL_WRITE_PATH=dslite, or the first path's tool is
    missing and the route lists DSLite second."""
    import os
    import toolfind
    if not available(board):
        return False
    if os.environ.get("FRL_WRITE_PATH") == "dslite":
        return True
    kind = "ccs_scripting" if board["target"].get("transport") == "dss" else board["target"].get("openocd", "openocd")
    try:
        toolfind.find(kind)
        return False
    except Exception:          # the first path's tool is missing: DSLite, if it is installed, takes the write
        try:
            toolfind.find("dslite")
            return True
        except Exception:
            return False
