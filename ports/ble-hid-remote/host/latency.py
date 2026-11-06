#!/usr/bin/env python3
"""Log the board's serial lines and the Music app's play/pause events on one Mac clock,
and estimate button-to-action latency from them.

  ~/.venvs/ble/bin/python host/latency.py [SECONDS] [LOGFILE]   (default 600 s)

How it times a press. The firmware prints "HID tx n=.. bits=0x01 .." on its UART right after
it hands the press to the Bluetooth stack. This script stamps that line with time.time() when it
arrives over the XDS110 USB serial port. Music posts the distributed notification
com.apple.Music.playerInfo when it starts, pauses or changes track; the script stamps that too.
Latency = Music stamp - UART stamp. That is the radio leg plus macOS and Music, minus the UART
leg (a 40-byte line at 115200 baud is 3.5 ms, plus the XDS110's USB delivery), so the true
button-to-action time is this figure plus a few milliseconds. Needs no permissions: no key
logging, no event tap. Only presses that change Music's state can be timed.

  --strip FILE   just print a captured log without the terminal escape codes
  SERIAL_DEV=none  log Music events only (no board access, so no board lock needed); the press
                   times then come from macOS's own Bluetooth log, see the README
"""
import os
import re
import sys
import termios
import threading
import time

DEV = os.environ.get("SERIAL_DEV", "/dev/cu.usbmodemLS470GTU1")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def serial_lines(dev, out, stop):
    fd = os.open(dev, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    buf = b""
    while not stop.is_set():
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            time.sleep(0.001)
            continue
        if not chunk:
            time.sleep(0.001)
            continue
        now = time.time()
        buf += chunk
        # Display lines end with \r\n or are separated by cursor moves; split on both
        parts = re.split(rb"\r\n|\n|\x1b\[\d+;\d+H", buf)
        buf = parts.pop()
        for p in parts:
            s = ANSI.sub("", p.decode("ascii", "replace")).strip()
            if s:
                out.append((now, "UART " + s))
    os.close(fd)


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--strip":
        for line in open(sys.argv[2], errors="replace"):
            print(ANSI.sub("", line.rstrip()))
        return 0
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 600
    logfile = sys.argv[2] if len(sys.argv) > 2 else None
    events, stop = [], threading.Event()
    if DEV != "none":
        threading.Thread(target=serial_lines, args=(DEV, events, stop), daemon=True).start()

    import objc
    from Foundation import NSDate, NSDistributedNotificationCenter, NSObject, NSRunLoop

    class Obs(NSObject):
        def music_(self, note):
            info = note.userInfo() or {}
            events.append((time.time(), "MUSIC %s | %s" % (info.get("Player State"), info.get("Name"))))

    obs = Obs.alloc().init()
    NSDistributedNotificationCenter.defaultCenter().addObserver_selector_name_object_(
        obs, objc.selector(obs.music_, signature=b"v@:@"), "com.apple.Music.playerInfo", None)
    print("listening %.0f s on %s and Music; press the buttons" % (secs, DEV), flush=True)
    t_end, shown = time.time() + secs, 0
    while time.time() < t_end:
        NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.05))
        while shown < len(events):
            t, s = events[shown]
            print("%.3f %s" % (t, s), flush=True)
            shown += 1
    stop.set()
    events.sort()
    if logfile:
        with open(logfile, "w") as f:
            for t, s in events:
                f.write("%.3f %s\n" % (t, s))
    # pair each press (bits 0x01 or 0x02 going down) with the first Music event within 2 s
    lat = []
    for i, (t, s) in enumerate(events):
        m = re.search(r"HID tx n=\d+ bits=0x(0[12])", s)
        if not m:
            continue
        nxt = next(((t2, s2) for t2, s2 in events[i + 1:] if s2.startswith("MUSIC") and t2 - t <= 2.0), None)
        if nxt:
            lat.append((m.group(1), (nxt[0] - t) * 1000))
    for bits, ms in lat:
        print("press bits=0x%s -> Music %.1f ms" % (bits, ms))
    if lat:
        v = sorted(ms for _, ms in lat)
        print("n=%d  median %.1f ms  min %.1f  max %.1f" % (len(v), v[len(v) // 2], v[0], v[-1]))
    else:
        print("no press matched a Music event")
    return 0


if __name__ == "__main__":
    sys.exit(main())
