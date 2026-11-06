#!/usr/bin/env python3
"""Control-rate capture on the AM13 bench guard (guard version 7, revision H; our code). The guard records up to four
channels at the 20 kHz control rate into a ring of 2048 int16 samples (frl_guard.h, FRL_CAP_*), for what the host poll
at about 280 samples a second cannot see: back-EMF zero crossings against the Hall edges while coasting, the current-loop
step response. Capturing never changes control.

Readout is through the MotorVision window: the guard shows sample capIndex of each channel in capWin0..3 between two
sequence marks (capWinIdxB, the four values, capWinIdxA); with capWinMs set it steps capIndex itself, so the host only
streams the six variables and keeps every sample whose marks agree. A second path reads the raw ring over the debug probe
with DSLite (am13e230/tools/run_image.py --no-flash); a DSLite memory read has left the core halted before (2026-10-08),
so it is off unless asked for, and only with the motor stopped.

Run under the probe lock:
  python3 tools/frl/lock.py exec --serial AL130001 --op motor-capture -- python3 tools/motorlink/capture.py run OUTDIR TAG \\
      [--chans IA,IB,IC,VBUS] [--decim 1] [--len 2048] [--pre 0] [--trig now|stop|start] [--wait 30] [--dslite]
  python3 tools/motorlink/capture.py read OUTDIR TAG [--dslite]     read out a capture that is already done
  python3 tools/motorlink/capture.py channels                         list the channel names
Writes OUTDIR/TAG.capture.json (settings, scales, samples in units) and OUTDIR/TAG.capture.csv (one row per sample).
"""
import argparse, json, os, pathlib, re, struct, subprocess, sys, time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import campaign as C
import link as L

ROOT = L.ROOT
HEADER = ROOT / "am13e230" / "foc-drv8376" / "frl_guard.h"
IMAGE = pathlib.Path.cwd() / "out/am13e230/foc-drv8376/foc.out"
TRIGS = {"now": 0, "stop": 1, "start": 2}
STATES = {0: "idle", 1: "armed", 2: "capturing", 3: "done"}
WIN = ["frlGuard.capWinIdxB", "frlGuard.capWin[0]", "frlGuard.capWin[1]", "frlGuard.capWin[2]", "frlGuard.capWin[3]", "frlGuard.capWinIdxA"]
SETTINGS = ["frlGuard.capState", "frlGuard.capCount", "frlGuard.capLen", "frlGuard.capPre", "frlGuard.capDecim", "frlGuard.capTrig",
            "frlGuard.capStart", "frlGuard.capChan[0]", "frlGuard.capChan[1]", "frlGuard.capChan[2]", "frlGuard.capChan[3]"]
SCALES = [f"frlGuard.capScale[{i}]" for i in range(4)]


def channels():
    """channel names and ids from frl_guard.h (FRL_CAP_IA = 1, ...), so the host cannot drift from the firmware"""
    found = {m.group(1): int(m.group(2)) for m in re.finditer(r"FRL_CAP_(\w+)\s*=\s*(\d+)", HEADER.read_text())}
    return {n: i for n, i in found.items() if n not in ("CHANNELS", "IDLE", "ARMED", "CAPTURING", "DONE")}


def chan_id(x):
    ids = channels()
    if str(x).isdigit():
        return int(x)
    if x.upper() not in ids:
        raise SystemExit(f"{x}: not a capture channel; known: {', '.join(ids)}")
    return ids[x.upper()]


def need_v7(k):
    if k.board_version() is None or k.board_version() < 7:
        raise SystemExit(f"the board reports guard version {k.board_version()!r}; the capture buffer needs version 7 or later")


def arm(k, chans=("IA", "IB", "IC", "VBUS"), decim=1, length=2048, pre=0, trig="now"):
    """write the settings and arm; returns the settings as the guard read them back (it clamps out-of-range values)"""
    need_v7(k)
    ids = [chan_id(c) for c in chans] + [0] * (4 - len(chans))
    for i, c in enumerate(ids[:4]):
        k.write(f"frlGuard.capChan[{i}]", c)
    k.write("frlGuard.capDecim", int(decim)); k.write("frlGuard.capLen", int(length)); k.write("frlGuard.capPre", int(pre))
    k.write("frlGuard.capTrig", TRIGS.get(str(trig), trig)); k.write("frlGuard.capWinMs", 0)
    k.write("frlGuard.capArm", 1); k.read(0.05)
    got = C.snapshot(k, SETTINGS, 0.4)
    if got.get("capState") == 0:
        raise SystemExit(f"arming refused (the guard refuses while it calibrates or mid-capture): {got}")
    return got


def cancel(k):
    k.write("frlGuard.capArm", 2); k.read(0.05)


def wait_done(k, secs=30.0):
    """poll capState until done; the polls themselves keep the link alive"""
    t0 = time.time()
    while time.time() - t0 < secs:
        s = C.snapshot(k, ["frlGuard.capState", "frlGuard.capCount"], 0.25)
        if s.get("capState") == 3:
            return s
    raise SystemExit(f"capture not done after {secs} s: {s}")


def _packets(chunks, by_index):
    """every POLLING_DATA packet in arrival order: (index, value)"""
    data = b"".join(b for _, b in chunks)
    out, i, n = [], 0, len(data)
    while True:
        i = data.find(bytes([L.START, L.POLLING_DATA]), i)
        if i < 0 or i + 4 > n:
            return out
        v = by_index.get(data[i + 2])
        m = L.SIZE[v["type"]] if v else 0
        if v and i + 3 + m < n and data[i + 3 + m] == L.END:
            out.append((data[i + 2], struct.unpack(L.FMT[v["type"]], data[i + 3:i + 3 + m])[0])); i += 4 + m
        else:
            i += 1


def read_window(k, count, step_ms=4, passes=4):
    """stream the window while the guard steps capIndex every step_ms; keep each sample whose two marks agree. Samples a
    pass missed are read one at a time at the end. Returns {index: [ch0, ch1, ch2, ch3]} in units."""
    vs = [L.find(k.vars, n) for n in WIN]
    idx = [v["index"] for v in vs]
    got = {}
    k.write("frlGuard.capIndex", 0); k.write("frlGuard.capWinMs", int(step_ms))
    try:
        for _ in range(passes):
            C.start_stream(k, WIN)
            col = C.Collector(k)
            col.pump(count * step_ms / 1000.0 + 0.5)          # one pass; the heartbeat keeps the board streaming
            k.send(L.STOP_POLLING); col.pump(0.1, beat=False)
            seq = _packets(col.chunks, {v["index"]: v for v in vs})
            for j in range(len(seq) - 5):
                if [p[0] for p in seq[j:j + 6]] == idx and seq[j][1] == seq[j + 5][1] and seq[j][1] < count:
                    got[seq[j][1]] = [seq[j + 1 + c][1] for c in range(4)]
            if len(got) >= count:
                break
    finally:
        k.write("frlGuard.capWinMs", 0)
    for i in range(count):                                    # the rest, one by one
        if i in got:
            continue
        k.write("frlGuard.capIndex", i); k.read(0.01)
        for _ in range(3):
            r = k.poll(WIN, 0.15)
            if r[WIN[0]] == i and r[WIN[5]] == i:
                got[i] = [r[WIN[1 + c]] for c in range(4)]; break
    return got


def read_dslite(start, count, scales, serial=os.environ.get("PROBE"), image=IMAGE):
    """the raw ring through the debug probe: frlCapBuf, 4 x 2048 int16, address from the image's map. Motor stopped only:
    a DSLite memory read has halted the core before (am13e230/tools/run_image.py)."""
    n = 2048
    r = subprocess.run([sys.executable, str(ROOT / "am13e230/tools/run_image.py"), str(image), "0", f"frlCapBuf:{4 * n * 2}",
                        "--no-flash", "--serial", serial], capture_output=True, text=True, timeout=120)
    m = re.search(r"frlCapBuf @0x([0-9a-f]+): ([0-9a-f]+)", r.stdout)
    if not m:
        raise SystemExit("DSLite read failed: " + (r.stdout + r.stderr)[-400:])
    raw = bytes.fromhex(m.group(2))
    vals = [int.from_bytes(raw[2 * j:2 * j + 2], "little", signed=True) for j in range(4 * n)]
    return {i: [vals[c * n + (start + i) % n] * scales[c] for c in range(4)] for i in range(count)}


def save(outdir, tag, meta, samples):
    out = pathlib.Path(outdir); out.mkdir(parents=True, exist_ok=True)
    names = {v: n for n, v in channels().items()}
    cols = [names.get(meta["settings"].get(f"capChan[{c}]"), f"ch{c}") for c in range(4)]
    rate = 20000.0 / max(1, meta["settings"].get("capDecim") or 1)
    rows = [[i, round((i - (meta["settings"].get("capPre") or 0)) / rate, 7)] + samples[i] for i in sorted(samples)]
    (out / f"{tag}.capture.json").write_text(json.dumps(dict(meta, columns=["sample", "t_s"] + cols, rows=rows), indent=1) + "\n")
    (out / f"{tag}.capture.csv").write_text("sample,t_s," + ",".join(cols) + "\n" + "".join(",".join(str(x) for x in r) + "\n" for r in rows))
    return out / f"{tag}.capture.json"


def readout(k, outdir, tag, dslite=False, serial=os.environ.get("PROBE")):
    s = C.snapshot(k, SETTINGS, 0.4); sc = C.snapshot(k, SCALES, 0.3)
    if s.get("capState") != 3:
        raise SystemExit(f"no finished capture on the board: {s}")
    scales = [sc.get(f"capScale[{c}]") for c in range(4)]
    n = s["capCount"]
    t0 = time.time(); samples = read_window(k, n); t1 = time.time()
    meta = {"settings": s, "scales": scales, "count": n, "window_s": round(t1 - t0, 2), "read": "window",
            "missing": [i for i in range(n) if i not in samples], "guard_version": k.board_version()}
    if dslite:
        g = C.snapshot(k, ["frlGuard.state", "motorVars_M1.inputCtrl.flagEnableRunAndIdentify"], 0.3)
        if g.get("state") in (1, 2) or g.get("flagEnableRunAndIdentify"):
            raise SystemExit(f"not reading over the probe while the motor runs or calibrates: {g}")
        raw = read_dslite(s["capStart"], n, scales, serial)
        meta["dslite_agrees"] = all(abs(raw[i][c] - samples[i][c]) <= 1e-6 + abs(scales[c]) / 2 for i in samples for c in range(4))
    return save(outdir, tag, meta, samples), samples, meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["run", "read", "channels"])
    ap.add_argument("outdir", nargs="?"); ap.add_argument("tag", nargs="?")
    ap.add_argument("--chans", default="IA,IB,IC,VBUS"); ap.add_argument("--decim", type=int, default=1)
    ap.add_argument("--len", type=int, default=2048); ap.add_argument("--pre", type=int, default=0)
    ap.add_argument("--trig", default="now", choices=list(TRIGS)); ap.add_argument("--wait", type=float, default=30.0)
    ap.add_argument("--dslite", action="store_true"); ap.add_argument("--serial", default=os.environ.get("PROBE"), required=not os.environ.get("PROBE"))
    a = ap.parse_args()
    if a.cmd == "channels":
        for n, i in channels().items(): print(i, n)
        return
    k = L.Link()
    try:
        if a.cmd == "run":
            print("armed", arm(k, a.chans.split(","), a.decim, a.len, a.pre, a.trig), flush=True)
            print("done", wait_done(k, a.wait), flush=True)
        p, samples, meta = readout(k, a.outdir, a.tag, a.dslite, a.serial)
        print("wrote", p, len(samples), "samples in", meta["window_s"], "s; missing", len(meta["missing"]))
    finally:
        k.close()


if __name__ == "__main__":
    main()
