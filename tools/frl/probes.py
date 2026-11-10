"""List debug probes and select one on purpose.

A probe serial says which XDS110 to talk to. It does not say which board is behind it: an external
probe can be moved between boards, and many LaunchPad kits share one USB identity. The board is checked
separately (target.check_identity) before any write.

Rules: with a serial given, that serial must be on USB, and every later operation uses it. With none
given, exactly one probe is selected as the only probe; two or more stop before any target access.
Enumeration order never picks a probe.
"""
import re
import subprocess
import sys
from pathlib import Path

import toolfind
from core import ROOT, stop

SERIAL = re.compile(r"^[A-Za-z0-9._-]{1,32}$")


def list_probes():
    """[{serial, name, via}] for XDS110 probes on USB. xdsdfu when present, else the OS USB list."""
    x = toolfind.find("xdsdfu", required=False)
    if x:
        try:
            out = subprocess.run([x["exe"], "-e"], capture_output=True, text=True, timeout=30).stdout
        except (OSError, subprocess.TimeoutExpired):
            out = ""
        probes, name = [], ""
        for line in out.splitlines():
            m = re.match(r"\s*Device Name:\s*(.+)", line)
            if m:
                name = m.group(1).strip()
            m = re.match(r"\s*Serial Num:\s*(\S+)", line)
            if m:
                probes.append({"serial": m.group(1), "name": name or "XDS110", "via": "xdsdfu"})
    else:
        probes = []
    # xdsdfu skips a probe another debugger holds open at that moment (seen 2026-11-08: 7 of 10 listed, then 12 of 12), so the
    # OS list is merged in rather than used only as a fallback. The two agree on serials; xdsdfu's names are kept.
    sys.path.insert(0, str(ROOT / "tools" / "benchmark"))
    try:
        import detect
        devs = detect.usb_devices()
    except Exception:
        devs = []
    seen = {p["serial"] for p in probes}
    for d in devs:
        if str(d.get("vid", "")).lower() == "0451" and str(d.get("pid", "")).lower() in ("bef3", "bef4") and d.get("serial") and d["serial"] not in seen:
            probes.append({"serial": d["serial"], "name": d.get("name") or "XDS110", "via": "usb"}); seen.add(d["serial"])
    return probes


def select(requested=None, probes=None):
    """The serial to use, or a Stop. Never chooses by order."""
    if requested is not None and not SERIAL.match(requested):
        stop("refused", "FRL-E070", f"probe serial {requested!r} has characters a serial number never has",
             "copy the serial exactly from `frl detect`")
    probes = list_probes() if probes is None else probes
    serials = sorted({p["serial"] for p in probes if p["serial"]})
    if requested:
        if requested not in serials:
            stop("refused", "FRL-E003", f"probe {requested} is not on USB (found: {', '.join(serials) or 'none'})",
                 "check the USB cable is a data cable and the board's power LED is on, then run `frl detect`",
                 probes=probes)
        return requested
    if not serials:
        stop("refused", "FRL-E001", "no XDS110 debug probe found on USB",
             "plug the board in with a data cable (a charge-only cable powers the board but hides it), "
             "straight into the computer rather than a hub, then run `frl detect`", probes=probes)
    if len(serials) > 1:
        stop("refused", "FRL-E002", f"{len(serials)} probes on USB: {', '.join(serials)}",
             "name one with --probe <serial>. To find which is which, unplug one board and run `frl detect` again",
             probes=probes)
    return serials[0]
