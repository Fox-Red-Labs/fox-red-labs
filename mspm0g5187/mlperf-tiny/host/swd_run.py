#!/usr/bin/env python3
"""Drive the SWD_HARNESS image of the MLPerf Tiny keyword-spotting port without the UART: for each clip of the kws01
folder (make_kws01.py), write its 490 bytes into gp_buff through OpenOCD's memory port, set swd.go, wait for it to clear,
read the 12 int8 logits and the cycle count. The core is never halted. Same data and the same infer 70 / 10 warm-up
loop count as the benchmark's performance script, 5 windows, median throughput.
  python3 swd_run.py <image .out> <kws01 folder> <probe serial> <out json> [clips]"""
import sys, os, socket, subprocess, time, json, csv, statistics
img, ds, probe, outj = sys.argv[1:5]; nclips = int(sys.argv[5]) if len(sys.argv) > 5 else 1000
OCD = os.path.expanduser("~/ti/ti-openocd"); NM = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm"
sym = {l.split()[2]: int(l.split()[0], 16) for l in subprocess.run([NM, img], capture_output=True, text=True).stdout.splitlines() if len(l.split()) == 3}
SW, GP = sym["swd"], sym["gp_buff"]
p = subprocess.Popen([OCD + "/bin/openocd", "-s", OCD + "/share/openocd/scripts", "-f", "interface/xds110.cfg", "-c", f"adapter serial {probe}", "-c", "adapter speed 2500",
                      "-f", "target/ti_mspm0.cfg", "-c", "gdb_port disabled; telnet_port disabled; tcl_port 6666", "-c", "init"], stdout=open("/tmp/swd_ocd.log", "w"), stderr=subprocess.STDOUT)
time.sleep(5); s = socket.create_connection(("127.0.0.1", 6666))
def tcl(c):
    s.sendall(c.encode() + b"\x1a"); b = b""
    while not b.endswith(b"\x1a"): b += s.recv(65536)
    return b[:-1].decode().strip()
def rd(a, n): return [int(x, 0) for x in tcl(f"read_memory 0x{a:x} 32 {n}").split()]
def run(clip, warm, n):
    tcl(f"write_memory 0x{GP:x} 8 {{{' '.join(str(b) for b in clip)}}}"); tcl(f"write_memory 0x{SW+4:x} 32 {{0 {warm} {n}}}")  # go cleared, warm, n
    tcl(f"write_memory 0x{SW+4:x} 32 {{1 {warm} {n}}}")
    for _ in range(20000):
        w = rd(SW, 6)
        if w[1] == 0: break
    else: raise RuntimeError("no completion")
    ob = tcl(f"read_memory 0x{SW+20:x} 8 12").split()
    return w[4], [(int(x, 0) - 256 if int(x, 0) > 127 else int(x, 0)) for x in ob]
assert rd(SW, 1)[0] == 0x5EDA0001, "harness not running"
rows = [l.split(",") for l in open(os.path.join(ds, "y_labels.csv"))][:nclips]
t0 = time.time(); ok = 0; cyc = []; preds = []
wins = []
clk = 80e6
c0 = open(os.path.join(ds, rows[0][0].strip()), "rb").read()
for w in range(5):
    cy, _ = run(c0, 10, 70); wins.append(70 / (cy / clk))
for i, r in enumerate(rows):
    clip = open(os.path.join(ds, r[0].strip()), "rb").read()
    cy, o = run(clip, 0, 1); pred = max(range(12), key=lambda k: o[k]); preds.append(pred); cyc.append(cy); ok += (pred == int(r[2]))
res = dict(image=os.path.basename(os.path.dirname(img)), clips=len(rows), correct=ok, accuracy=ok / len(rows), windows_inf_per_s=wins, median_inf_per_s=statistics.median(wins),
           median_ms=1e3 / statistics.median(wins), single_call_cycles_min=min(cyc), single_call_cycles_median=statistics.median(cyc), single_call_cycles_max=max(cyc), seconds=time.time() - t0, preds=preds)
json.dump(res, open(outj, "w")); print({k: v for k, v in res.items() if k != "preds"})
tcl("shutdown"); p.wait(timeout=10)
