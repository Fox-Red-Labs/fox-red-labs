#!/usr/bin/env python3
"""Flash the SimpleFOC image, wait for the unattended run to end, read g_log once, save it and print the analysis.
   run_simplefoc.py <simplefoc.out> --serial AL130001 --out <folder> [--wait 70] [--no-flash]
The run ends with the driver off, so the single probe read (which halts the core) cannot leave a voltage on the windings.
Take the bench lock first: mkdir <scratch>/locks/lp-am13e230"""
import argparse, ctypes as C, glob, json, os, re, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
DSL = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
NE, NS, NI = 1700, 700, 2800
class RunLog(C.Structure):
    _fields_ = [("n_edge", C.c_uint32), ("n_samp", C.c_uint32), ("n_cur", C.c_uint32), ("loops", C.c_uint32), ("loop_cyc_min", C.c_uint32),
                ("loop_cyc_max", C.c_uint32), ("loop_cyc_sum", C.c_uint64), ("status", C.c_uint32), ("t_start_us", C.c_uint32),
                ("hold_start_ms", C.c_uint32), ("hold_end_ms", C.c_uint32), ("edge_us", C.c_uint32 * NE), ("edge_st", C.c_uint8 * NE),
                ("vel", C.c_float * NS), ("tgt", C.c_float * NS), ("uq", C.c_float * NS), ("ipk_mA", C.c_int16 * NI)]
class Log(C.Structure):
    _fields_ = [("magic", C.c_uint32), ("version", C.c_uint32), ("done", C.c_uint32), ("vm_mV", C.c_uint32), ("rest", C.c_int32 * 3),
                ("cpu_hz", C.c_uint32), ("run", RunLog * 2)]
STATUS = {0: "none", 1: "done", 2: "no bus voltage", 3: "nFAULT low at wake", 4: "overcurrent trip", 5: "no Hall edge", 6: "bus out of range",
          7: "nFAULT during run", 8: "driver init failed", 9: "sensor align failed"}

def analyse(L, run_i, f_ref=20.0, pp=4):
    r = L.run[run_i]; out = {"status": STATUS.get(r.status, r.status), "loops": r.loops, "n_edge": r.n_edge}
    if r.loops:
        out["loop_us_mean"] = r.loop_cyc_sum / r.loops / (L.cpu_hz / 1e6); out["loop_us_min"] = r.loop_cyc_min / (L.cpu_hz / 1e6); out["loop_us_max"] = r.loop_cyc_max / (L.cpu_hz / 1e6)
    t = [r.edge_us[i] / 1e6 for i in range(r.n_edge)]
    h0, h1 = r.hold_start_ms / 1000.0, r.hold_end_ms / 1000.0
    w0, w1 = h0 + 3.0, h1               # steady window: skip 3 s after the ramp ends (TI's rows used 13 to 18 s of a 20 s run)
    ed = [x for x in t if w0 <= x <= w1]
    out["edges_in_window"] = len(ed)
    if len(ed) > 14:
        # speed per electrical revolution (6 edges) and per sector (1 edge), electrical Hz
        rev = [6.0 / 6.0 / (ed[i + 6] - ed[i]) for i in range(len(ed) - 6)]       # 1 rev / time for 6 edges
        sec = [1.0 / 6.0 / (ed[i + 1] - ed[i]) for i in range(len(ed) - 1)]
        mean = (len(ed) - 1) / 6.0 / (ed[-1] - ed[0])
        out["mean_Hz_elec"] = mean; out["rpm_mech"] = mean * 60.0 / pp
        out["ripple_rev_pct"] = (max(rev) - min(rev)) / mean * 100.0
        out["ripple_sector_pct"] = (max(sec) - min(sec)) / mean * 100.0
        # rev-to-rev speed, one value per 6 consecutive edges stepping by 6 (independent windows)
        ind = [1.0 / (ed[i + 6] - ed[i]) for i in range(0, len(ed) - 6, 6)]
        out["ripple_indep_rev_pct"] = (max(ind) - min(ind)) / mean * 100.0
        if len(ind) > 2:
            m = sum(ind) / len(ind); out["rms_indep_rev_pct"] = (sum((x - m) ** 2 for x in ind) / len(ind)) ** 0.5 / mean * 100.0
    ip = [r.ipk_mA[i] for i in range(r.n_cur)]
    iw = ip[int(w0 * 200): int(w1 * 200)]
    if iw: out["ipk_window_max_mA"] = max(iw); out["ipk_window_mean_mA"] = sum(iw) / len(iw); out["ipk_run_max_mA"] = max(ip)
    uq = [r.uq[i] for i in range(r.n_samp)][int(w0 * 50): int(w1 * 50)]
    if uq: out["uq_window_mean_V"] = sum(uq) / len(uq)
    v = [r.vel[i] for i in range(r.n_samp)][int(w0 * 50): int(w1 * 50)]
    if v: out["simplefoc_vel_window_mean_Hz_elec"] = sum(v) / len(v) * pp / 6.2831853
    return out

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("image"); ap.add_argument("--serial", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--wait", type=float, default=70); ap.add_argument("--no-flash", action="store_true"); a = ap.parse_args()
    ccxml = os.path.join(HERE, "..", "..", "..", "am13e230", "AM13E23019_%s.ccxml" % a.serial)
    mp = open(glob.glob(os.path.join(os.path.dirname(os.path.abspath(a.image)), "*.map"))[0]).read()
    addr = int(re.search(r"^\s*([0-9a-f]{8})\s+g_log\s*$", mp, re.M).group(1), 16)
    os.makedirs(a.out, exist_ok=True)
    if not a.no_flash:
        r = subprocess.run([DSL, "flash", "-c", ccxml, "-e", "-f", "-v", "-r", "1", "-u", "-t", "60", a.image], capture_output=True, text=True, timeout=120)
        open(os.path.join(a.out, "flash.log"), "w").write(r.stdout + r.stderr)
        if "verification successful" not in r.stdout: print("flash failed, see flash.log"); sys.exit(1)
    t1 = time.time(); print("flashed; the run takes about 60 s; waiting %.0f s" % a.wait, file=sys.stderr)
    time.sleep(a.wait)
    nbytes = (C.sizeof(Log) + 3) // 4 * 4
    binp = os.path.join(a.out, "g_log.bin")
    subprocess.run([DSL, "memory", "-c", ccxml, "-r", "0x%x,%d" % (addr, nbytes), "-s", "8", "-o", binp], capture_output=True, text=True, timeout=120)
    raw = open(binp, "rb").read()
    L = Log.from_buffer_copy(raw[:C.sizeof(Log)])
    res = {"magic_ok": L.magic == 0x534F4643, "done": hex(L.done), "vm_V": L.vm_mV / 1000.0, "rest_counts": list(L.rest), "elapsed_s": round(time.time() - t1, 1),
           "open_loop": analyse(L, 0), "closed_loop": analyse(L, 1)}
    json.dump(res, open(os.path.join(a.out, "analysis.json"), "w"), indent=1)
    print(json.dumps(res, indent=1))
if __name__ == "__main__": main()
