#!/usr/bin/env python3
"""Flash a board image of the m33-whx engine (build.sh with BOARD=1), wait for the three demos, read the RAM struct fb_res over the
probe and print the figures. Run it under the probe lock:
   python3 tools/frl/lock.py exec --serial M3010001 -- python3 -I ports/doom/m33-whx/tools/run_board.py <doom.out> <label>
The reference values (frame-checksum chain after each demo, frames, gametic, cache counters) come from the emulator run of the same
sources: results/board-run-summary.txt. Frame time is CPU cycles in the frame loop with the checksum time left out, from the DWT counter."""
import json, os, struct, subprocess, sys, tempfile, time
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
DSL = "/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite"
CCXML = os.path.join(REPO, "mspm33c321a", "MSPM33C321A_M3010001.ccxml")
NM = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmnm"
FMT = "@9I" "2I" "I" "3I3I3I" "3Q3Q" "3I3I" "3I3I3I3I" "3I3I" "5I" "96s"
NAMES = ("magic state fault whx_base whx_bytes crc_expect crc_got selfcheck_ms10 selfcheck_reads frames demos_done chain_crc "
         "demo_chain demo_frames demo_gametic demo_cyc demo_qcyc demo_qreads demo_qbytes demo_fast demo_slow demo_set demo_miss "
         "demo_fmax demo_qmax init_cyc crc_hi crc_lo cpu_hz log_n log").split()
GROUPS = {"demo_chain": 3, "demo_frames": 3, "demo_gametic": 3, "demo_cyc": 3, "demo_qcyc": 3, "demo_qreads": 3, "demo_qbytes": 3,
          "demo_fast": 3, "demo_slow": 3, "demo_set": 3, "demo_miss": 3, "demo_fmax": 3, "demo_qmax": 3}

def run(a): return subprocess.run([DSL] + a, capture_output=True, text=True)
def sym(out, name):
    for l in subprocess.check_output([NM, out]).decode().splitlines():
        p = l.split()
        if len(p) == 3 and p[2] == name: return int(p[0], 16)
    raise SystemExit("no symbol " + name)
def read(addr, n):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        r = run(["memory", "-c", CCXML, "-r", "0x%x,%d" % (addr, (n + 3) // 4), "-s", "8", "-o", f.name])
        if r.returncode: raise RuntimeError(r.stdout[-300:])
        return open(f.name, "rb").read()[:n]
def parse(b):
    v = list(struct.unpack_from(FMT, b)); d = {}; i = 0
    for nm in NAMES:
        k = GROUPS.get(nm)
        if k: d[nm] = v[i:i + k]; i += k
        else: d[nm] = v[i]; i += 1
    d["log"] = d["log"].split(b"\0")[0].decode("latin1")
    return d

def main():
    out, label = sys.argv[1], sys.argv[2]
    addr = sym(out, "fb_res"); size = struct.calcsize(FMT)
    t0 = time.time()
    r = run(["flash", "-c", CCXML, "-e", "-f", "-v", out])
    if r.returncode: raise SystemExit("flash failed: " + r.stdout[-400:] + r.stderr[-300:])
    print("flashed in %.1f s; fb_res at 0x%x" % (time.time() - t0, addr), flush=True); t1 = time.time()
    d = None
    while time.time() - t1 < 1500:
        time.sleep(12)
        d = parse(read(addr, size))
        print("  t=%4.0f s state %d frames %d demos %d fault %d" % (time.time() - t1, d["state"], d["frames"], d["demos_done"], d["fault"]), flush=True)
        if d["state"] in (4, 9): break
    wall = time.time() - t1
    d["wall_s_after_flash"] = wall; d["label"] = label
    open(os.path.join(os.path.dirname(os.path.abspath(out)), "fb_res-%s.json" % label), "w").write(json.dumps(d, indent=1))
    print(json.dumps({k: (["%08x" % x for x in v] if k == "demo_chain" else v) for k, v in d.items() if k not in ("demo_cyc", "demo_qcyc")}))
    hz = 160e6
    print("whx in flash: crc %08x expect %08x (%s); self-check read %d pages in %.1f ms" % (d["crc_got"], d["crc_expect"], "match" if d["crc_got"] == d["crc_expect"] else "MISMATCH", d["selfcheck_reads"], d["selfcheck_ms10"] / 10))
    pf = 0; pc = 0; pq = 0
    for i in range(min(d["demos_done"], 3)):
        fr = d["demo_frames"][i] - pf; cyc = d["demo_cyc"][i] - pc; q = d["demo_qcyc"][i] - pq
        acc = (d["demo_fast"][i] + d["demo_slow"][i]) - ((d["demo_fast"][i - 1] + d["demo_slow"][i - 1]) if i else 0)
        mis = d["demo_miss"][i] - (d["demo_miss"][i - 1] if i else 0)
        print("demo %d: frames %d gametic %d chain %08x | frame loop %.2f ms/frame (%.1f frames/s) of which QSPI %.2f ms | reads %d, misses/frame %.0f%s" % (
            i + 1, fr, d["demo_gametic"][i], d["demo_chain"][i], cyc / hz * 1e3 / (fr - (1 if i == 0 else 0)), (fr - (1 if i == 0 else 0)) / (cyc / hz), q / hz * 1e3 / (fr - (1 if i == 0 else 0)),
            d["demo_qreads"][i] - (d["demo_qreads"][i - 1] if i else 0), (d["demo_qreads"][i] - (d["demo_qreads"][i - 1] if i else 0)) / fr,
            (" | accesses %d misses %d hit %.3f %%" % (acc, mis, 100 * (1 - mis / acc))) if acc else ""))
        pf, pc, pq = d["demo_frames"][i], d["demo_cyc"][i], d["demo_qcyc"][i]
    tf = d["frames"] - 1; tc = d["demo_cyc"][2] if d["demos_done"] >= 3 else 0
    if tc: print("all three: %d frames, %.2f ms/frame mean, %.1f frames/s, QSPI %.2f ms/frame; longest frame %.1f ms, most QSPI in one frame %.1f ms; init %.0f ms; checksum chain %08x; wall %.0f s" % (
        d["frames"], tc / hz * 1e3 / tf, tf / (tc / hz), d["demo_qcyc"][2] / hz * 1e3 / tf, max(d["demo_fmax"]) / hz * 1e3, max(d["demo_qmax"]) / hz * 1e3, d["init_cyc"] / hz * 1e3, d["chain_crc"], wall))
main()
