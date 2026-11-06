#!/usr/bin/env python3
"""Read the EDUMKII image's state over the probe (run under the probe lock) and draw what the panel is being sent.
   python3 tools/frl/lock.py exec --serial M3010001 -- python3 -I ports/doom/m33-whx/tools/edu_dump.py <doom.out> <out_prefix> [--frames]
Prints the edu_res counters (struct in plat/edu.c). With --frames also reads both 320 x 168 frame buffers and the palette, writes
<out_prefix>-fb0.pgm / -fb1.pgm (indices as grey) and <out_prefix>-view.ppm (the first buffer through the palette at 128 x 80, the panel's
scaler written again on the host), so a wrong palette or a blank frame is told apart from a wrong panel set-up."""
import os, struct, subprocess, sys, tempfile
DSL = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
CCXML = os.path.join(REPO, "mspm33c321a", "MSPM33C321A_M3010001.ccxml")
NM = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm"
NAMES = "magic frames joy_x joy_y joy_x0 joy_y0 buttons stream_cyc_last stream_cyc_max stall_cyc_total conv_cyc_total hud_draws fps10 mode pal_changes starts_game".split()

def sym(out, name):
    for l in subprocess.check_output([NM, out]).decode().splitlines():
        p = l.split()
        if len(p) == 3 and p[2] == name: return int(p[0], 16)
    raise SystemExit("no symbol " + name)
def read(addr, n):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        r = subprocess.run([DSL, "memory", "-c", CCXML, "-r", "0x%x,%d" % (addr, (n + 3) // 4), "-s", "8", "-o", f.name], capture_output=True, text=True)
        if r.returncode: raise SystemExit("read failed: " + r.stdout[-300:])
        return open(f.name, "rb").read()[:n]

def main():
    out, pre = sys.argv[1], sys.argv[2]
    b = read(sym(out, "edu_res"), 4 * len(NAMES))
    d = dict(zip(NAMES, struct.unpack("<%dI" % len(NAMES), b)))
    print(" ".join("%s=%d" % (k, v) for k, v in d.items()))
    print("stream last %.2f ms max %.2f ms; engine stall %.1f ms per frame; conversion %.2f ms per frame; fps %.1f" % (
        d["stream_cyc_last"] / 160e3, d["stream_cyc_max"] / 160e3, d["stall_cyc_total"] / 160e3 / max(d["frames"], 1),
        d["conv_cyc_total"] / 160e3 / max(d["frames"], 1), d["fps10"] / 10))
    g = struct.unpack("<8i", read(sym(out, "fox_edu_dbg"), 32))
    print("engine: gamestate %d (0 level, 3 demo screen) player x %d y %d angle %d health %d ammo %d key events %d gametic %d" % g)
    if "--frames" in sys.argv:
        fb = read(sym(out, "frame_buffer"), 2 * 320 * 168)
        pal = read(sym(out, "pal"), 512)
        for i in (0, 1): open("%s-fb%d.pgm" % (pre, i), "wb").write(b"P5 320 168 255\n" + fb[i * 53760:(i + 1) * 53760])
        P = struct.unpack("<256H", pal)
        def rgb(v): return ((v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3)
        rows = bytearray()
        for j in range(80):
            y = (j * 336 + 168) // 160; s = fb[y * 320:(y + 1) * 320]; x = 0
            for i in range(128):
                a, c = P[s[x >> 1]], P[s[(x >> 1) + 1]]; x += 5
                m = ((a >> 1) & 0x7BEF) + ((c >> 1) & 0x7BEF); rows += bytes(rgb(m))
        open(pre + "-view.ppm", "wb").write(b"P6 128 80 255\n" + bytes(rows))
        rows = bytearray()                      # the page mapping: 200 rows over 96, rows 168 and up from the other buffer
        for j in range(96):
            y = (j * 400 + 200) // 192; s = fb[y * 320:(y + 1) * 320] if y < 168 else fb[53760 + (y - 32) * 320:53760 + (y - 31) * 320]; x = 0
            for i in range(128):
                a, c = P[s[x >> 1]], P[s[(x >> 1) + 1]]; x += 5
                m = ((a >> 1) & 0x7BEF) + ((c >> 1) & 0x7BEF); rows += bytes(rgb(m))
        open(pre + "-page.ppm", "wb").write(b"P6 128 96 255\n" + bytes(rows))
        print("palette[0..3] %s; wrote %s-fb0.pgm -fb1.pgm -view.ppm -page.ppm" % ([hex(x) for x in P[:4]], pre))
main()
