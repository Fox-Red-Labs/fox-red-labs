"""Checks chosen by board, recipe and the result asked for. A check reports what it measured; it never
claims a person saw anything.

pattern: the asked-for LED on/off times, matched against pin transitions timed through the debugger.
The unchanged factory blink (0.5 s on, 0.5 s off) fails a changed-pattern check, as it should.
"""
import os
import re

from core import Result


def parse_pattern(text):
    """'150,150,150,900' -> [150, 150, 150, 900]: on, off, on, off ... in milliseconds."""
    try:
        ms = [int(x) for x in str(text).replace(" ", "").split(",") if x]
    except ValueError:
        return None, "the pattern is a list of whole milliseconds, on then off: 150,150,150,900"
    if len(ms) < 2 or len(ms) % 2:
        return None, "the pattern needs pairs of on and off times, for example 150,150,150,900"
    if len(ms) > 16:
        return None, "at most 8 on/off pairs"
    if any(m < 50 or m > 5000 for m in ms):
        return None, "each time must be between 50 and 5000 ms, so a person and the check can both see it"
    if sum(ms) > 6000:
        return None, "one cycle of the pattern must fit in 6 seconds"
    return ms, ""


def segments(transitions):
    """Whole levels between transitions: [(level, ms)]. The first entry is the level at the first read, so
    the level it starts is partial and dropped; the level after the last change is never closed."""
    segs = [(b0, t1 - t0) for (t0, b0), (t1, _) in zip(transitions, transitions[1:])]
    return segs[1:]


def match(pattern, segs, tol_ms):
    """True when the observed levels repeat the pattern for at least one full cycle, in some phase."""
    want = [(1 if i % 2 == 0 else 0, ms) for i, ms in enumerate(pattern)]
    n = len(want)
    if len(segs) < n:
        return False, 0
    best = 0
    for start in range(len(segs)):
        for phase in range(n):
            k = 0
            while start + k < len(segs):
                lvl, ms = segs[start + k]
                wl, wms = want[(phase + k) % n]
                if lvl != wl or abs(ms - wms) > max(tol_ms, 0.1 * wms):
                    break
                k += 1
            best = max(best, k)
            if k >= n:
                return True, k
    return False, best


def pattern_check(sample, pattern, tol_ms, label, min_rate=100):
    """Result for a pattern check from target.sample() output. A route whose probe reads slowly (the C2000 through
    CCStudio, about 30 reads a second while the CPU runs) sets a lower min_rate; the tolerance then widens to two
    sample periods, and the result says so."""
    trans, n, span = sample["transitions"], sample["samples"], sample["span_ms"]
    rate = n / max(span, 1) * 1000
    segs = segments(trans)
    data = {"led": label, "samples": n, "span_ms": span, "samples_per_s": round(rate),
            "transitions": len(trans), "levels_ms": [ms for _, ms in segs][:24], "asked_ms": pattern,
            "method": "LED pin output register read through the debugger and timed on this computer; nobody looked at the LED"}
    if sample.get("timed") is False:
        data["method"] = ("LED pin output-enable and output registers read through DSLite after a reset; nobody looked at the LED. "
                          "Each read takes seconds, so the timing was not measured")
        data["levels_seen"] = sample.get("levels_seen")
        if sample["output_enabled"] is None:
            return Result.make("unverified", "FRL-E042", f"frl could not read whether {label} is set as an output",
                               "run `frl verify` again; if it repeats, look at the LED yourself", **data)
        if not sample["output_enabled"]:
            return Result.make("failed", "FRL-E040", f"{label} is not set as an output, so the program on the board is not driving it",
                               "flash the pattern image with `frl flash`, then `frl verify`", **data)
        if len(sample.get("levels_seen") or []) < 2:
            return Result.make("failed", "FRL-E040", f"{label} stayed at one level in {n} reads, so the program is not blinking it",
                               "flash the pattern image with `frl flash`, then `frl verify`", **data)
        return Result.good(f"{label} is an output and moved between on and off ({n} reads); the pattern timing was not measured "
                           "because a debugger read of this part takes seconds", **data)
    if rate < 100 and rate >= min_rate:
        tol_ms = max(tol_ms, round(2000 / rate))
        data["method"] += f"; {rate:.0f} reads a second, so the tolerance is two sample periods ({tol_ms} ms)"
    if rate < min_rate:
        return Result.make("unverified", "FRL-E042", f"only {rate:.0f} reads a second through the probe: too slow to time a pattern",
                           "close other debug sessions and run `frl verify` again", **data)
    if sample["output_enabled"] is None:
        return Result.make("unverified", "FRL-E042", f"frl could not read whether {label} is set as an output",
                           "run `frl verify` again; if it repeats, look at the LED yourself", **data)
    if not sample["output_enabled"]:
        return Result.make("failed", "FRL-E040", f"{label} is not set as an output, so the program on the board is not driving it",
                           "flash the pattern image with `frl flash`, then `frl verify`", **data)
    if sample.get("pin_agrees") is False:
        return Result.make("failed", "FRL-E040", f"{label}'s output register disagrees with the program's own log, so the log does not prove the pin moved",
                           "flash the pattern image with `frl flash`, then `frl verify`", **data)
    if "pin_agrees" in sample and sample["pin_agrees"] is None:
        return Result.make("unverified", "FRL-E042", f"frl could not read {label}'s output register, so the log is not tied to the pin",
                           "run `frl verify` again; if it repeats, look at the LED yourself", **data)
    if sum(pattern) * 1.5 > span:
        return Result.make("unverified", "FRL-E042", "the check did not run long enough for one and a half cycles",
                           "run `frl verify --seconds <longer>`", **data)
    ok, k = match(pattern, segs, tol_ms)
    if ok:
        return Result.good(f"{label} repeats the asked pattern {','.join(map(str, pattern))} ms (timed {len(segs)} levels, tolerance {tol_ms} ms or 10%)", **data)
    seen = ", ".join(f"{'on' if lvl else 'off'} {ms}" for lvl, ms in segs[:8]) or "no change at all"
    return Result.make("failed", "FRL-E040", f"{label} does not follow the asked pattern; it did: {seen} (ms)",
                       "the image on the board is not the one asked for: rebuild and flash, or check the pattern in the source", **data)


def parse_message(text):
    """The person's message for print-message: plain printable ASCII, 1 to 40 characters."""
    t = str(text or "")
    if not 1 <= len(t) <= 40:
        return None, "the message must be 1 to 40 characters"
    if any(not (32 <= ord(ch) < 127) or ch in '"\\' for ch in t):
        return None, "use plain letters, digits, spaces and punctuation, without quotes or backslashes"
    return t, ""


def read_serial(port, baud, seconds):
    """Bytes read from a serial port for `seconds`, standard library only (termios; macOS and Linux)."""
    import termios
    import time as _t
    fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        speed = getattr(termios, f"B{baud}")
        attrs[0] = 0                                   # iflag: raw
        attrs[1] = 0                                   # oflag
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0                                   # lflag: no echo, no canonical
        attrs[4] = attrs[5] = speed
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIFLUSH)
        out, end = b"", _t.time() + seconds
        while _t.time() < end:
            try:
                chunk = os.read(fd, 4096)
                out += chunk
            except BlockingIOError:
                pass
            _t.sleep(0.02)
        return out
    finally:
        os.close(fd)


def message_check(raw, message, port):
    """Result for a print-message check: the message arrived at least twice, with its counter moving."""
    text = raw.replace(b"\x00", b"").decode("ascii", "replace")   # TI's example also sends each string's closing NUL
    text = re.sub(r"\x1b(\[[0-9;?]*[A-Za-z]|[78DEM])", "\n", text)   # ANSI cursor and color codes (TI's Display driver) end a line
    lines = [ln.strip() for ln in text.replace("\r", "\n").split("\n") if ln.strip()]
    hits = [ln for ln in lines if ln.startswith(f"> {message} ")]
    data = {"port": port, "bytes": len(raw), "lines": lines[-6:], "asked": message,
            "method": "read the board's USB serial port at 115200 baud; nobody looked at a terminal"}
    if not raw:
        return Result.make("failed", "FRL-E040", f"nothing arrived on {port} in the check's time",
                           "check J16 and J17 are on XDS (right), then flash again", **data)
    if len(hits) >= 2 and len({h.rsplit(' ', 1)[-1] for h in hits}) >= 2:
        return Result.good(f"the board printed \"> {message} N\" {len(hits)} times on {port}, counting", **data)
    shown = next((ln for ln in reversed(lines) if ln.startswith("> ")), lines[-1] if lines else "(nothing readable)")
    return Result.make("failed", "FRL-E040", f"the serial port shows other text, not your message: {shown[:60]}",
                       "the program on the board is not the one asked for: rebuild and flash", **data)
