#!/usr/bin/env python3
"""parse_rec.py <names comma-separated> < run_image.py output : turn the h264_rec hex dump into the same H264R lines the console boards print."""
import re, struct, sys
names = sys.argv[1].split(",")
txt = sys.stdin.read()
m = re.search(r"h264_rec @0x[0-9a-f]+: ([0-9a-f]+)", txt); b = bytes.fromhex(m.group(1))
F = "magic stream pass w h frames crc_first crc_last crc_all rgb_all cyc_total cyc_idr cyc_p cyc_pmax cyc_rgb peak_heap allocs inbuf concealed status".split()
for i in range(0, len(b) - 79, 80):
    v = dict(zip(F, struct.unpack("<20I", b[i:i + 80])))
    if v["magic"] != 0x48323634: continue
    print("H264R s=%s p=%d st=%d %dx%d fr=%d crc=%08x/%08x/%08x rgb=%08x cyc=%d idr=%d p=%d pmax=%d rgbc=%d heap=%d in=%d allocs=%d conc=%d" % (
        names[v["stream"]], v["pass"], v["status"], v["w"], v["h"], v["frames"], v["crc_first"], v["crc_last"], v["crc_all"], v["rgb_all"],
        v["cyc_total"], v["cyc_idr"], v["cyc_p"], v["cyc_pmax"], v["cyc_rgb"], v["peak_heap"], v["inbuf"], v["allocs"], v["concealed"]))
