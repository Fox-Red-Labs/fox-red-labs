#!/usr/bin/env python3
"""make_rows.py: turn the saved H264R logs into rows of data/results/<board>.csv (test video_sw_decode_fps and video_sw_decode_ram), replacing earlier video_decode rows.
Run from the repo root.  Logs: LOGS (default out/h264/logs). Formulas for the computed and does_not_fit rows are in the notes."""
import csv, os, re, statistics as st, sys
LOGS = os.environ.get("LOGS", os.path.abspath("out/h264/logs")); DATE = "2026-10-10"; SRC = "portability/h264/SOURCE.md"
def parse(fn):
    rows = {}
    for l in open(os.path.join(LOGS, fn), errors="replace"):
        m = re.search(r"H264R s=(\S+) p=(\d+) st=(\d+) (\d+)x(\d+) fr=(\d+) crc=(\w+)/(\w+)/(\w+) rgb=(\w+) cyc=(\d+) idr=(\d+) p=(\d+) pmax=(\d+) rgbc=(\d+) heap=(\d+) in=(\d+) allocs=(\d+) conc=(\d+)", l)
        if m: rows.setdefault(m.group(1), []).append(m.groups())
    return rows
def vname(s, opt):
    opt = opt.lower()
    m = re.match(r"(?:thermo|prof-baseline|prof-main|bbb)-(\d+x\d+)", s); wh = m.group(1)
    if s.startswith("prof-baseline"): return "h264-%s-ref1-600k30-%s" % (wh, opt)
    if s.startswith("bbb"): return "h264-%s-bbb-%s" % (wh, opt)
    return "h264-%s-ref1-%s" % (wh, opt)
def strip_bytes(w, sb): rows = max(2, min(16, (sb // 2 // w) & ~1)); return rows * w * 2
def mbs(w, h): return ((w + 15) // 16) * ((h + 15) // 16)
def heap(w, h): return 988 * mbs(w, h) + 4160          # fitted on the measured heaps of 176x144, 240x240, 320x240, 480x270, 540x540, 640x360 (32-bit pointers)
BOARDS = {
 "am263p": dict(clk=400, cond="compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;route=H264R lines on UART0 console, port opened before the RAM load, no flash written;h264bsd 42bcb5d with parameter-set tables cut to 2, harness portability/h264;PMU cycle counter around the decoder calls, RGB565 strips timed apart;code streams buffers in OCSRAM 2 MB layout, caches on", checked="crc", stack="stack not counted (16,384 B reserved, not measured)", strip=10240, avail=2031616, avail_note="2,031,616 B OCSRAM layout (0x70000000 to 0x701EFFFF, E147)",
   logs=[("stream-os-r1.txt", "Os"), ("profile-os-r1.txt", "Os"), ("bbb-os-r1.txt", "Os"), ("stream-o3-r1.txt", "O3"), ("profile-o3-r1.txt", "O3")]),
 "mspm33c321a": dict(clk=160, cond="compiler=ticlang-5.1.1.LTS;sdk=mspm33_sdk_1_04_00_00;machine=macos-15.7.9-x86_64;route=records in RAM (h264_rec) read back over the XDS110 with DSLite, probe M3010001;h264bsd 42bcb5d with parameter-set tables cut to 2, harness portability/h264;DWT CYCCNT around the decoder calls;code and streams run from flash (2 wait states, cache on), buffers in the 256 kB SRAM;TI demo backed up and restored", checked="crc", stack="stack not counted (5,680 B used at -Os, 7,600 B at -O3, measured)", strip=10240, avail=262144, avail_note="262,144 B SRAM (code and streams are in flash)",
   logs=[("mspm33-s240-Os.txt", "Os"), ("mspm33-s240-O3.txt", "O3")]),
 "am13e230": dict(clk=200, cond="compiler=ticlang-5.1.1.LTS;sdk=am13e230x_sdk_26_01_00_03;machine=macos-15.7.9-x86_64;route=records in RAM (h264_rec) read back over the XDS110 with DSLite, probe AL130001;h264bsd 42bcb5d with parameter-set tables cut to 2, harness portability/h264;DWT CYCCNT around the decoder calls;code and streams run from flash, buffers in the 96 kB data RAM plus 32 kB code RAM (RAM_C, used as a second heap pool where noted)", checked="crc", stack="stack not counted (5,680 B used at -Os, 7,588 B at -O3, measured)", strip=2816, avail=131072, avail_note="131,072 B = 96 kB data RAM plus 32 kB code-optimized RAM",
   logs=[("am13e230-s176-Os.txt", "Os"), ("am13e230-s176-O3.txt", "O3"), ("am13e230-ram96-Os.txt", "Os")]),
 "cc35x1": dict(clk=160, cond="compiler=ticlang-5.1.1.LTS;sdk=simplelink_wifi_sdk_10_20_00_39;toolbox=4.3.23;machine=macos-15.7.9-x86_64;route=H264R lines on the application UART2 (115200), probe E10000FQ;FreeRTOS thread, Wi-Fi and BLE stacks not started;h264bsd 42bcb5d with parameter-set tables cut to 2, harness portability/h264;DWT CYCCNT (SysTick fallback) around the decoder calls;code and streams in external flash (cache), buffers in the M33 DRAM", checked="serial", stack="stack not counted (thread stack 12,288 B)", strip=10240, avail=458267, avail_note="458,267 B = the M33 DRAM of 520,783 B (0x28000DB0 to 0x2807FFFF in TI's linker file) less 62,516 B of other .bss in the image; the 128 kB TCM_DRAM was not used",
   logs=[("cc35x1-s320-Os.txt", "Os")]),
}

NOFIT = {  # stream name -> (WxH, inbuf bytes measured on the Mac)
 "stream-320x240": (320, 240, 5248), "stream-540x540": (540, 540, 14224), "prof-baseline-480x270": (480, 270, 11680), "prof-baseline-640x360": (640, 360, 12096),
 "prof-baseline-1280x720": (1280, 720, 14128), "prof-baseline-1920x1080": (1920, 1080, 17088), "stream-240x240": (240, 240, 4928), "stream-176x144": (176, 144, 3280)}
out = {}
def add(b, r): out.setdefault(b, []).append(r)
def row(b, test, var, status, value, hi, runs, clk, cond, checked, note): return [b, test, var, status, value, hi, runs, clk, cond, DATE, SRC, checked, note]
for b, c in BOARDS.items():
    ran = set()
    for fn, opt in c["logs"]:
        if not os.path.exists(os.path.join(LOGS, fn)):
            if b == "cc35x1":
                for wh in ("176x144", "240x240", "320x240"):
                    add(b, row(b, "video_sw_decode_fps", "h264-%s-ref1-%s" % (wh, opt.lower()), "waiting", "", "", "", c["clk"], c["cond"], "", "image built and signed (cc35x1/bench/build_h264.sh, 292,764 B code and constants, 374,516 B bss); the chip's debug port is stuck (error -615) until the J12 power cycle"))
            continue
        for s, r in parse(fn).items():
            g = r[0]; w, h, fr = int(g[3]), int(g[4]), int(g[5])
            if int(g[2]) != 0:
                if b == "am263p" and opt == "Os": add(b, row(b, "video_sw_decode_fps", "h264-%dx%d-main-cabac-bframes" % (640, 360), "not_supported", "", "", 3, c["clk"], c["cond"], "serial", "h264bsd refuses a Main-profile stream (profile_idc 77, CABAC, B-frames): h264bsdDecode returns H264BSD_ERROR (3) on the first parameter set, on the board and on the Mac; it decodes Baseline only. OpenH264 decodes Main and High"))
                continue
            ran.add((s, opt))
            cyc = st.median(int(x[10]) for x in r); rgb = st.median(int(x[14]) for x in r); pf = cyc / fr; px = w * h; fps = c["clk"] * 1e6 / pf
            ok = len({(x[6], x[7], x[8], x[9]) for x in r}) == 1
            sb = strip_bytes(w, c["strip"]); ram = int(g[15]) + int(g[16]) + sb
            note = "cycles per frame %.2fM; %.1f cycles per pixel; RGB565 strips add %.1f cycles per pixel (%.1f fps with them); YUV and RGB CRCs equal the Mac's%s; median of %d runs, spread %d cycles" % (pf / 1e6, pf / px, rgb / fr / px, c["clk"] * 1e6 / (pf + rgb / fr), "" if ok else " (runs differ!)", len(r), max(int(x[10]) for x in r) - min(int(x[10]) for x in r))
            v = vname(s, opt)
            add(b, row(b, "video_sw_decode_fps", v, "measured", "%.1f" % fps, "", len(r), c["clk"], c["cond"], c["checked"], note))
            add(b, row(b, "video_sw_decode_ram", v, "measured", ram, "", len(r), c["clk"], c["cond"], c["checked"], "decoder heap peak %d B + input NAL buffer %d B + RGB565 strip %d B; %s" % (int(g[15]), int(g[16]), sb, c["stack"])))
    done_streams = {s for s, _ in ran}
    for s, (w, h, inb) in NOFIT.items():
        if s in done_streams: continue
        need = heap(w, h) + inb + strip_bytes(w, c["strip"])
        if need <= c["avail"]: continue
        kind = "prof-baseline" if s.startswith("prof") else "thermo"
        v = vname(s, "os")
        add(b, row(b, "video_sw_decode_ram", v, "does_not_fit", c["avail"] - need, "", "", c["clk"], c["cond"], "linker" if False else "by-us",
           "needs %d B (heap 988 x %d macroblocks + 4,160, fitted on the measured heaps; input buffer %d; strip %d) against %s; code, stack and streams not counted" % (need, mbs(w, h), inb, strip_bytes(w, c["strip"]), c["avail_note"])))
# AM263P: 720p and 1080p, computed rate if memory allowed
cpp_hi = 66.7; cpp_lo = 35.9   # measured R5F -Os cycles per pixel: 176x144 (high) and the 600 kbit/s 640x360 stream (low)
for wh, (w, h) in {"1280x720": (1280, 720), "1920x1080": (1920, 1080)}.items():
    fps_lo = 400e6 / (w * h * cpp_hi); fps_hi = 400e6 / (w * h * cpp_lo)
    add("am263p", row("am263p", "video_sw_decode_fps", "h264-%s-ref1-600k30-os" % wh, "computed", "%.1f" % fps_lo, "%.1f" % fps_hi, "", 400, BOARDS["am263p"]["cond"], "by-us",
       "if memory allowed: 400e6 / (%d x %d pixels x cycles per pixel), with 66.7 (measured 176x144) to 35.9 (measured 640x360 at 600 kbit/s)" % (w, h)))
for b in out:
    fn = "data/results/%s.csv" % b; keep = []
    with open(fn, newline="") as f:
        rd = list(csv.reader(f))
    hdr, body = rd[0], rd[1:]
    body = [r for r in body if not (len(r) > 1 and r[1].startswith("video_sw_decode"))]
    body += out[b]
    with open(fn, "w", newline="") as f:
        w = csv.writer(f); w.writerow(hdr); w.writerows(body)
    print(b, len(out[b]), "rows")
