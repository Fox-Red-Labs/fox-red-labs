#!/usr/bin/env python3
"""End-user demos for the CC1407P (radio_lab.c on the LP-MSPM0G5187) and the CC1354P10 (partner.c), run
and measured from the Mac. Each demo appends its result to site/data/radio-demos.json for the demo page.

    demos.py voice [--text "..."]   walkie-talkie, one way: Mac speech -> CC1354P10 -> air -> CC1407P -> Mac
    demos.py fox                    fox hunt: the beacon's signal, hot to cold, as its power steps down
    demos.py ring [--n 10]          key finder: ring the handset from the base, time the answer
    demos.py node --minutes 3       mailbox, doorbell, panic, messages and keep-close tag, with Home Assistant
    demos.py battery                shed sensor battery life, computed from the measured air time

Images: voice needs partner.c built for Wi-SUN mode 2b and radio_lab.c; the others need partner.c at
50 kbps 2-GFSK. Ports from LAB_G and LAB_P as for lab.py; Home Assistant as for range_ha.py.
"""
import argparse, base64, json, os, re, struct, subprocess, sys, tempfile, time, wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "cc1354p10", "radio-rx"))
import lab  # noqa: E402

DATA = os.path.join(ROOT, "site", "data", "radio-demos.json")
MEDIA = os.path.join(ROOT, "site", "media", "radio-demos")

# ---------- IMA ADPCM, 4 bits a sample (the classic codec in WAV files and many toys) ----------
STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
        1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
        8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8]


def adpcm_step(code, pred, idx):
    st = STEP[idx]; d = st >> 3
    if code & 4: d += st
    if code & 2: d += st >> 1
    if code & 1: d += st >> 2
    pred = pred - d if code & 8 else pred + d
    pred = max(-32768, min(32767, pred))
    idx = max(0, min(88, idx + IDX[code & 7]))
    return pred, idx


def adpcm_encode(samples, pred, idx):
    out, nib = bytearray(), []
    for s in samples:
        st = STEP[idx]; diff = s - pred; code = 0
        if diff < 0: code = 8; diff = -diff
        if diff >= st: code |= 4; diff -= st
        if diff >= st >> 1: code |= 2; diff -= st >> 1
        if diff >= st >> 2: code |= 1
        pred, idx = adpcm_step(code, pred, idx)
        nib.append(code)
    for i in range(0, len(nib), 2):
        out.append(nib[i] | ((nib[i + 1] if i + 1 < len(nib) else 0) << 4))
    return bytes(out), pred, idx


def adpcm_decode(data, pred, idx, n):
    out = []
    for b in data:
        for code in (b & 15, b >> 4):
            pred, idx = adpcm_step(code, pred, idx); out.append(pred)
    return out[:n]


def save_result(key, result):
    os.makedirs(os.path.dirname(DATA), exist_ok=True)
    d = json.load(open(DATA)) if os.path.exists(DATA) else {"note": "Written by mspm0g5187/radio/demos.py; one entry per demo, the latest run.", "demos": {}}
    result["run"] = time.strftime("%Y-%m-%d %H:%M")
    d["demos"][key] = result
    json.dump(d, open(DATA, "w"), indent=1)
    print(json.dumps(result, indent=1)[:1500])


def ha_client():
    if os.environ.get("HA_URL") and os.environ.get("HA_TOKEN"):
        import ha_bridge
        return ha_bridge._Direct(os.environ["HA_URL"], os.environ["HA_TOKEN"])
    import ha
    return ha


def ha_post(hac, eid, state, name, **attrs):
    a = {"friendly_name": name}; a.update(attrs)
    try:
        hac.req("POST", "/api/states/" + eid, {"state": state, "attributes": a})
    except Exception as e:  # keep the demo running if Home Assistant is away
        print("HA post failed:", e)


# ---------- voice ----------
def voice(a):
    os.makedirs(MEDIA, exist_ok=True)
    tmp = tempfile.mkdtemp()
    aiff, wav8 = os.path.join(tmp, "say.aiff"), os.path.join(tmp, "say8k.wav")
    subprocess.run(["say", "-o", aiff, a.text], check=True)
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@8000", "-c", "1", aiff, wav8], check=True)
    w = wave.open(wav8); pcm = list(struct.unpack("<%dh" % w.getnframes(), w.readframes(w.getnframes()))); w.close()
    N = 400                                     # 50 ms at 8 kHz -> 200 bytes of ADPCM
    frames, pred, idx = [], 0, 0
    for i in range(0, len(pcm), N):
        chunk = pcm[i:i + N] + [0] * max(0, N - len(pcm[i:i + N]))
        hdr = struct.pack("<HhB", len(frames), pred, idx)
        enc, pred, idx = adpcm_encode(chunk, pred, idx)
        frames.append(hdr + enc)
    log = open(a.log, "a")
    g, p = lab.Port("G", lab.G, log), lab.Port("P", lab.P, log)
    for t in g.run("P 0 0x0008"): print("G", t)
    g.send(f"D V {int(len(frames) * 50 + 8000)}")
    time.sleep(0.5)
    for t in p.run("V", 10): print("P", t)
    sent_t, got, got_t = {}, {}, {}
    t0 = time.time()
    for k, f in enumerate(frames):
        while time.time() < t0 + k * 0.050: g_read(g, got, got_t)
        p.send_raw("A" + base64.b64encode(f).decode())
        sent_t[k] = time.time()
        g_read(g, got, got_t)
    end = time.time() + 3
    while time.time() < end: g_read(g, got, got_t)
    p.send_raw("X"); p.lines(1.0)
    g.send("X"); tail = g.lines(3.0)
    done = [t for t in tail if t.startswith("DONE D V")]
    # rebuild: missing frames become silence
    out = []
    for k in range(len(frames)):
        if k in got:
            seq, pr, ix = struct.unpack("<HhB", got[k][:5]); fr = adpcm_decode(got[k][5:], pr, ix, N)
            out += fr + [0] * (N - len(fr))          # a short frame still fills its 50 ms, so later audio stays in place
        else:
            out += [0] * N
    ref = []
    for f in frames:
        seq, pr, ix = struct.unpack("<HhB", f[:5]); ref += adpcm_decode(f[5:], pr, ix, N)
    for name, data in (("voice-sent.wav", ref), ("voice-received.wav", out)):
        ww = wave.open(os.path.join(MEDIA, name), "w"); ww.setnchannels(1); ww.setsampwidth(2); ww.setframerate(8000)
        ww.writeframes(struct.pack("<%dh" % len(data), *data)); ww.close()
    lat = sorted((got_t[k] - sent_t[k]) * 1000 for k in got if k in sent_t)
    same = sum(1 for k in got if got[k] == frames[k])
    save_result("voice", {
        "title": "Walkie-talkie, one way", "text": a.text, "frames_sent": len(frames), "frames_received": len(got),
        "frames_bit_exact": same, "audio_seconds": round(len(frames) * 0.05, 2), "codec": "IMA ADPCM, 4 bits, 8 kHz (32 kbps)",
        "phy": "Wi-SUN FSK mode 2b, 100 kbps, 915 MHz", "frame_ms": 50, "frame_bytes": len(frames[0]),
        "latency_ms_median": round(lat[len(lat) // 2], 1) if lat else None, "latency_ms_max": round(lat[-1], 1) if lat else None,
        "radio_counters": done[0] if done else None, "sent_wav": "media/radio-demos/voice-sent.wav", "received_wav": "media/radio-demos/voice-received.wav"})


def g_read(g, got, got_t):
    for t in g.lines(0.005):
        m = re.match(r"A,(\d+),(-?\d+),([0-9a-f]+)", t)
        if m:
            b = bytes.fromhex(m.group(3))
            if len(b) >= 5:
                seq = struct.unpack("<H", b[:2])[0]
                if seq not in got: got[seq] = b; got_t[seq] = time.time()


# ---------- fox hunt ----------
def heat(rssi):
    if rssi is None: return "no signal"
    return "hot" if rssi > -40 else "warm" if rssi > -60 else "cool" if rssi > -80 else "cold"


def fox(a):
    log = open(a.log, "a")
    g, p = lab.Port("G", lab.G, log), lab.Port("P", lab.P, log)
    for t in g.run("P 13"): print("G", t)
    p.send("R"); p.lines(1.0)   # partner.c answers DONE R; rfrx.c ignores it
    steps = []
    for dbm, amp in ((14, 1), (10, 0), (4, 0), (0, 0), (-6, 0), (-12, 0), (-20, 0), (-30, 0)):
        g.run(f"W {dbm} {amp}"); p.lines(0.3)
        g.send("T 8 20 120")
        rs, end = [], time.time() + 4
        while time.time() < end:
            for t in p.lines(0.1):
                m = re.search(r"pkt len=20 rssi=(-?\d+) status=0x00 text=FRL P13", t) or \
                    re.search(r"pkt \d+ len=20 rssi=(-?\d+) status=0x00 data=46524c20503133", t)   # rfrx.c prints hex; "FRL P13"
                if m: rs.append(int(m.group(1)))
            if len(rs) >= 8: break
        g.lines(0.5)
        med = sorted(rs)[len(rs) // 2] if rs else None
        steps.append({"beacon_dbm": dbm, "amplifier": "high" if amp else "low", "heard": len(rs), "of": 8, "rssi": med, "heat": heat(med)})
        print(steps[-1])
    g.run("W 14 1")
    save_result("fox", {"title": "Fox hunt, hot and cold", "beacon": "CC1407P on the LP-MSPM0G5187, 2-GFSK 50 kbps, 915 MHz",
                        "hunter": "LP-CC1354P10-1", "method": "The beacon steps its power down, standing in for walking away; the walk in the yard is not done yet, and the boards sat on one desk",
                        "scale": {"hot": "above -40 dBm", "warm": "-60 to -40", "cool": "-80 to -60", "cold": "below -80"}, "steps": steps})


# ---------- key finder ----------
def ring(a):
    log = open(a.log, "a")
    g, p = lab.Port("G", lab.G, log), lab.Port("P", lab.P, log)
    for t in g.run("P 13"): print("G", t)
    g.send(f"D N {int(a.n * 6000 + 10000)}"); time.sleep(1.5)
    p.run("R", 10)
    tries = []
    for k in range(a.n):
        t0 = time.time(); found = None; sends = 0
        while time.time() - t0 < 5 and not found:
            first = p.run("S FRL RING", 5); sends += 1
            end = time.time() + 0.6
            while time.time() < end and not found:
                for t in first + p.lines(0.05):
                    m = re.search(r"text=FRL FOUND r=(-?\d+)", t)
                    if m and not found: found = (time.time(), int(m.group(1)), int(re.search(r"rssi=(-?\d+)", t).group(1)))
        tries.append({"n": k, "found": bool(found), "ms": round((found[0] - t0) * 1000) if found else None, "sends": sends,
                      "rssi_at_handset": found[1] if found else None, "rssi_at_base": found[2] if found else None})
        print(tries[-1]); time.sleep(2.2)    # the handset blinks for about 1.8 s after it answers
    g.send("X"); g.lines(2)
    ok = [t for t in tries if t["found"]]
    ms = sorted(t["ms"] for t in ok)
    save_result("ring", {"title": "Key finder: ring on demand", "base": "LP-CC1354P10-1, with the ring command sent from the computer", "handset": "CC1407P on the LP-MSPM0G5187, which blinks its LEDs",
                         "rings": len(tries), "answered": len(ok), "ms_median": ms[len(ms) // 2] if ms else None, "ms_max": ms[-1] if ms else None,
                         "tries": tries})


# ---------- node: mailbox, doorbell, panic, messages, keep-close tag ----------
def node(a):
    log = open(a.log, "a")
    g, p = lab.Port("G", lab.G, log), lab.Port("P", lab.P, log)
    hac = ha_client()
    for t in g.run("P 13"): print("G", t)
    g.send(f"D N {int(a.minutes * 60000)}"); time.sleep(1.0)
    p.run("R", 10)
    events, tags, last_tag, out_of_range, replies = [], 0, time.time(), False, 0
    end = time.time() + a.minutes * 60
    ha_post(hac, "binary_sensor.frl_tag_in_range", "on", "Fox Red Labs keep-close tag", device_class="presence")
    next_reply = time.time() + 20
    t_begin, stopped = time.time(), False
    while time.time() < end:
        if a.stop_after and not stopped and time.time() - t_begin > a.stop_after:
            g.send("X"); stopped = True; events.append({"t": time.time(), "what": "tag switched off (test)", "rssi": None}); print("tag switched off")
        for t in g.lines(0.05):
            if t.startswith(("EV,", "MSG,", "RING", "NODE")): print("G", t)
        for t in p.lines(0.05):
            m = re.search(r"rssi=(-?\d+) status=0x00 text=(FRL (EV|MSG|TAG)[^\r\n]*)", t)
            if not m: continue
            rssi, txt, kind = int(m.group(1)), m.group(2).strip(), m.group(3)
            now = time.time()
            if kind == "TAG":
                tags += 1; last_tag = now
                if out_of_range:
                    out_of_range = False; events.append({"t": now, "what": "tag back in range", "rssi": rssi})
                    ha_post(hac, "binary_sensor.frl_tag_in_range", "on", "Fox Red Labs keep-close tag", device_class="presence")
                ha_post(hac, "sensor.frl_tag_signal", rssi, "Fox Red Labs tag signal", unit_of_measurement="dBm", device_class="signal_strength") if tags % 5 == 0 else None
                continue
            ev = {"t": now, "what": txt[4:], "rssi": rssi}
            t0 = time.time()
            if "MAIL" in txt:
                ha_post(hac, "binary_sensor.frl_mailbox", "on" if "open" in txt else "off", "Fox Red Labs mailbox", device_class="opening")
            elif "BELL" in txt:
                ha_post(hac, "binary_sensor.frl_doorbell", "on", "Fox Red Labs doorbell", device_class="occupancy")
                ha_post(hac, "binary_sensor.frl_doorbell", "off", "Fox Red Labs doorbell", device_class="occupancy")
            elif "PANIC" in txt:
                ha_post(hac, "binary_sensor.frl_panic", "on", "Fox Red Labs panic button", device_class="safety")
            elif "MAGNET" in txt:
                ha_post(hac, "binary_sensor.frl_magnet", "on" if "near" in txt else "off", "Fox Red Labs magnet sensor", device_class="opening")
            elif kind == "MSG":
                ha_post(hac, "sensor.frl_kid_message", txt[8:], "Fox Red Labs kid message")
            ev["ha_ms"] = round((time.time() - t0) * 1000)
            events.append(ev); print("EVENT", ev)
        if not out_of_range and time.time() - last_tag > 3.5 and tags:
            out_of_range = True; events.append({"t": time.time(), "what": "tag out of range", "rssi": None})
            ha_post(hac, "binary_sensor.frl_tag_in_range", "off", "Fox Red Labs keep-close tag", device_class="presence")
            print("EVENT tag out of range")
        if a.reply and time.time() > next_reply:
            msg = ["FRL MSG Dinner in 5", "FRL MSG Coming!", "FRL MSG Stay there"][replies % 3]
            p.run("S " + msg, 5); replies += 1; events.append({"t": time.time(), "what": "parent sent: " + msg[8:], "rssi": None})
            next_reply = time.time() + 30
    g.send("X"); tail = g.lines(3)
    t_start = events[0]["t"] if events else time.time()
    for e in events: e["t"] = round(e["t"] - t_start, 2)
    save_result(a.key or "node", {"title": "Mailbox, doorbell, panic button, messages and keep-close tag", "minutes": a.minutes, "stop_after_s": a.stop_after,
                         "tags_heard": tags, "events": events, "handset_summary": [t for t in tail if t.startswith("DONE")]})


# ---------- battery ----------
def battery(a):
    # Measured: host-view transmit time of one 39-byte weather packet at 50 kbps (phy-matrix.txt, 20 B: 6.2 ms; 200 B: 35.2 ms).
    tx_ms = 6.2 + (39 - 20) * (35.2 - 6.2) / 180
    # Data sheet figures (computed, not measured here): CC1407P TX 22 mA at +14 dBm (SWRS301), standby 0.8 uA; MSPM0G5187 assumed 5 mA awake for 30 ms per report and 2 uA asleep.
    rows = []
    for every_s in (1.04, 10, 60, 300):
        q_tx = 22.0 * tx_ms / 1000; q_mcu = 5.0 * 0.030; q_sleep = (0.0008 + 0.002) * every_s
        avg_ma = (q_tx + q_mcu + q_sleep) / every_s
        rows.append({"report_every_s": every_s, "avg_uA": round(avg_ma * 1000, 1),
                     "days_on_2_AA_2500mAh": round(2500 / avg_ma / 24), "days_on_CR2032_225mAh": round(225 / avg_ma / 24)})
    save_result("battery", {"title": "Far shed sensor: battery life", "status": "computed", "tx_ms_measured": round(tx_ms, 1),
                            "assumptions": "CC1407P 22 mA transmitting at +14 dBm and 0.8 uA standby (TI data sheet SWRS301); MSPM0G5187 5 mA for 30 ms per report and 2 uA asleep (assumed, not measured); sensors and regulator losses not counted",
                            "rows": rows})


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("demo"); ap.add_argument("--log", default="demos.log")
    ap.add_argument("--text", default="Hi Mom! This is Fox Red Labs, talking over a nine hundred fifteen megahertz radio. Over.")
    ap.add_argument("--n", type=int, default=10); ap.add_argument("--minutes", type=float, default=3); ap.add_argument("--reply", action="store_true")
    ap.add_argument("--stop-after", type=float, default=0, help="node: switch the tag off after this many seconds"); ap.add_argument("--key", default=None)
    a = ap.parse_args()
    {"voice": voice, "fox": fox, "ring": ring, "node": node, "battery": battery}[a.demo](a)


if __name__ == "__main__":
    main()
