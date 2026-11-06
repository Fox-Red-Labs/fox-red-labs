# "Touch to paint" for the Waveshare 3.5 inch panel on the LP-MSPM0G3519, in MicroPython.
# Run after lib/lcd35.py (paste both with tools/paste_raw.py). Touch a colour to pick it, touch
# the paper to draw, touch CLEAR to wipe. Prints and shows the full-screen fill time and the
# time of one touch read.
import time
from lcd35 import *   # when pasted as one script, drop this line

def paint():
    scr = Screen(20000000)
    tp = Touch()
    t = time.ticks_us()
    scr.fill(0xFFFF)
    fill_ms = time.ticks_diff(time.ticks_us(), t) / 1000
    t = time.ticks_us()
    n = 50
    for _ in range(n):
        tp.read()
    read_us = time.ticks_diff(time.ticks_us(), t) // n
    print("fill ms", fill_ms, "touch read us", read_us, "touch ids", tp.ids())
    colors = (0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F, 0x8010, 0x0000)
    bw = 40
    scr.text("TOUCH TO PAINT", 8, 6, 0x0000, 0xFFFF, 2)
    for i, c in enumerate(colors):
        scr.rect(i * bw, 30, bw, 40, c)
    scr.rect(7 * bw, 30, W - 7 * bw, 40, 0xC618)
    scr.text("CLR", 7 * bw + 4, 44, 0x0000, 0xC618, 1)
    scr.text("FILL %d MS  TOUCH %d US" % (round(fill_ms), read_us), 8, H - 14, 0x7BEF, 0xFFFF, 1)
    color = colors[0]
    last = None
    while True:
        p = tp.read()
        if p is None:
            last = None
            continue
        x, y = p
        if y < 76:
            if y >= 30 and last is None:
                i = x // bw
                if i < len(colors):
                    color = colors[i]
                elif i >= 7:
                    scr.rect(0, 76, W, H - 76 - 16, 0xFFFF)
            last = p
            continue
        x = min(max(x, 3), W - 4)
        y = min(max(y, 79), H - 20)
        scr.rect(x - 3, y - 3, 7, 7, color)
        last = p

paint()
