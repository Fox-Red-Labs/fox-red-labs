#!/usr/bin/env python3
"""Render site/radar-eval.html: the customer-facing antenna-configuration comparison for the IWRL6432 radar,
built only from iwrl6432boost/eval/results/*.summary.json (latest date per config). Anything not measured says so.
  python3 iwrl6432boost/eval/render_eval.py           write the page
  python3 iwrl6432boost/eval/render_eval.py --check   exit 1 if the page is out of date
"""
import glob, html, json, os, statistics, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import render_boards as rb  # noqa: E402
import site_ui as ui  # noqa: E402
esc = html.escape
OUT = os.path.join(ROOT, "site", "radar-eval.html")
ORDER = [("1x1", "1 transmit, 1 receive"), ("2x2", "2 transmit, 2 receive"), ("2x3", "2 transmit, 3 receive, all of them"), ("2x2-both", "2 transmit, 2 receive, run again with the Wi-Fi sensor logging at the same time")]
NM = '<span class="nm">not measured yet</span>'

def load():
    best = {}
    for p in sorted(glob.glob(os.path.join(HERE, "results", "*.summary.json"))):
        S = json.load(open(p))
        best.setdefault(S["config"], []).append(S)
    return best

def med(xs):
    xs = [x for x in xs if x is not None]
    return statistics.median(xs) if xs else None

def merged(sums):
    P, acc, sc = {}, None, None
    for S in sums:
        for k, v in S.get("passes", {}).items(): P.setdefault(k, []).extend(v)
        acc = S.get("accept") or acc; sc = S.get("stream_check") or sc
    return P, acc, sc

def metrics(sums):
    P, acc, sc = merged(sums)
    nf = sc is not None and sc.get("frames") == 0
    m = {"accepted": None if acc is None else (not acc.get("not_done") and not nf), "refused": bool(acc and acc.get("not_done")), "nostream": bool(acc and not acc.get("not_done") and nf), "rate": sc.get("rate_hz") if sc else None,
         "bytes": sc.get("bytes_per_frame") if sc else None}
    wall = P.get("walk", [])
    m["walk_on_start"] = sum(1 for x in wall if x.get("already_on_at_start"))
    w = [x for x in wall if not x.get("already_on_at_start")]   # a walk that starts already flagged says nothing about arrival range
    m["walk_n"] = len(w); m["walk"] = med([x.get("detect_range_m") for x in w])
    sits = {k: v for k, v in P.items() if k.startswith("sit")}
    held = [(float(k[3:]), med([x.get("hold_on_fraction") for x in v])) for k, v in sits.items()]
    ok = [d for d, f in held if f is not None and f >= 0.8]
    m["sit_far"] = max(ok) if ok else None; m["sit_tested"] = sorted(d for d, _ in held)
    allsit = [x for v in sits.values() for x in v]
    m["detect"] = med([x.get("detect_latency_s") for x in allsit] + [x.get("detect_t_s") for x in w])
    m["clear"] = med([x.get("clear_latency_s") for x in allsit] + [x.get("clear_latency_s") for x in w])
    e = P.get("empty", [])
    m["empty_s"] = sum(x.get("seconds", 0) for x in e); m["fa"] = sum(x.get("false_alarms", 0) for x in e)
    return m

def fa_cell(m):
    if not m["empty_s"]: return NM
    mins = m["empty_s"] / 60
    if m["fa"]: return f'{m["fa"]} in {mins:.0f} min ({m["fa"] / m["empty_s"] * 3600:.0f} per hour)'
    return f'0 in {mins:.0f} min (under {3 / m["empty_s"] * 3600:.0f} per hour at 95 percent)'

def row(cfg, label, sums, m):
    if sums is None:
        return f'<tr><td><b>{esc(cfg)}</b><br><span class="note">{esc(label)}</span></td><td colspan="7">{NM}</td></tr>'
    c = lambda v, f: NM if v is None else f.format(v)
    acc = "yes" if m["accepted"] else ("refused, see below" if m["refused"] else ("no frames, see below" if m["nostream"] else NM))
    walk = (f'already flagged at 8 m, the start of the test, so at least 8 m' if m["walk_on_start"] and m["walk"] is None else NM) if m["walk"] is None else f'{m["walk"]:.1f} m ({m["walk_n"]} pass{"es" if m["walk_n"] != 1 else ""})'
    if m["sit_far"] is None:
        seat = NM if not m["sit_tested"] else "not held at " + ", ".join(f"{d:g} m" for d in m["sit_tested"])
    else:
        seat = f'{m["sit_far"]:g} m' + (f', tested to {max(m["sit_tested"]):g} m' if m["sit_far"] == max(m["sit_tested"]) else "")
    return (f'<tr><td><b>{esc(cfg)}</b><br><span class="note">{esc(label)}</span></td><td>{acc}</td><td>{walk}</td><td>{seat}</td>'
            f'<td>{c(m["detect"], "{:.1f} s")}</td><td>{c(m["clear"], "{:.1f} s")}</td><td>{fa_cell(m)}</td><td>{NM}</td></tr>')

def render():
    data = load(); M = {c: metrics(data[c]) for c, _ in ORDER if c in data}
    measured = [c for c, _ in ORDER if c in M and (M[c]["walk"] is not None or M[c]["sit_far"] is not None or M[c]["empty_s"])]
    if not measured:
        ans = ("Nothing about range or speed is measured yet. The test tool and this page are built; the first runs are waiting for a person to walk in front of the radar. "
               "What is known now is which settings the radar's presence demo accepts.")
    else:
        ans = ("Measured so far. " + " ".join(
            f'{c}: ' + ", ".join(x for x in [
                f'walking seen at {M[c]["walk"]:.1f} m' if M[c]["walk"] is not None else "",
                f'seated held to {M[c]["sit_far"]:g} m' if M[c]["sit_far"] is not None else "",
                (f'{M[c]["fa"]} false alarms in {M[c]["empty_s"] / 60:.0f} empty minutes') if M[c]["empty_s"] else ""] if x) + "." for c in measured)
               + " Cells that say not measured yet have not been run.")
    rows = "".join(row(c, l, data.get(c), M.get(c)) for c, l in ORDER)
    table = ('<div class="tbl"><table><thead><tr><th>Antenna setting</th><th>Demo accepts it</th><th>Walking person first seen at</th><th>Seated person held to</th>'
             '<th>Detect delay</th><th>Clear delay</th><th>False alarms, empty room</th><th>Power</th></tr></thead><tbody>' + rows + '</tbody></table></div>')
    acc_notes = []
    for c, _ in ORDER:
        for S in data.get(c, []):
            nd = (S.get("accept") or {}).get("not_done")
            sc = S.get("stream_check")
            if not nd and sc is not None and sc.get("frames") == 0:
                acc_notes.append(f"<li>{esc(c)}: every line answered Done, but no frame came after sensorStart. This was the second setting started in that boot, and the demo then stopped answering. Whether the cause is the second start or the setting is not known; the shipped configuration (also 2x3) ran fine on its own boot. Each setting is to be run on its own boot.</li>")
            if not nd and sc and sc.get("frames"):
                acc_notes.append(f"<li>{esc(c)}: accepted; {esc(str(sc['frames']))} frames in the first 8 s at {esc(str(sc['rate_hz']))} a second, {esc(str(sc['bytes_per_frame']))} bytes a frame, {esc(str(sc['zones']))} zones.</li>")
            if nd: acc_notes.append(f'<li>{esc(c)}: {esc("; ".join(x["line"] + " answered " + (x["reply"] or "nothing") for x in nd[:3]))}</li>')
    accept_html = ("<ul>" + "".join(acc_notes) + "</ul>") if acc_notes else "<p>Every line of the configuration was answered Done for each setting that has been tried.</p>"
    rates = ", ".join(f'{c} {M[c]["rate"]:g} frames a second' + (f', {M[c]["bytes"]:g} bytes each' if M[c]["bytes"] else '') for c in M if M[c]["rate"])
    stk = [x for S in data.get("stock", []) for x in S.get("passes", {}).get("rate", [])]
    if stk: rates += ("; " if rates else "") + f'the shipped configuration on its own, 2x3 with lowPowerCfg 1 and outside the table, {stk[0]["rate_hz"]:g} frames a second'
    rates = rates and rates
    body = f"""<p class="crumb"><a href="index.html">Fox Red Labs</a> / Radar evaluation</p>
<h1 class="q">What do you get from all the antennas of a TI IWRL6432BOOST radar, against fewer, for presence detection?</h1>
<p class="answer">{esc(ans)}</p>
<p class="note">Terms: <a href="#words">words on this page</a> and <a href="../docs/vocab.md">the full list</a>. Board: <a href="../iwrl6432boost/README.md">IWRL6432BOOST</a>. How to read this and use it to choose a radar: <a href="../docs/guides/evaluate-a-presence-radar.md">the evaluation guide</a>. Run on TI's prebuilt Motion and Presence 2D demo (Radar Toolbox 4.00.00.05), macOS 15.7, 2026-10-10.</p>

<div class="blk" id="table">
<h2>The comparison</h2>
<p class="lede">One radar chip, three antenna settings. The setting is one line of the configuration, <code>channelCfg</code>.</p>
{table}
<p class="note">Walking: a person walks toward the radar from 8 m, with a call every 4 s. The column is the distance at the first two frames that flagged presence. Seated: a person sits still at each tested distance for 40 s. Held means presence on in at least 4 frames of 5. Delays run from our voice cue, so they include the person's reaction and walking time. Compare the rows to each other, not to a data sheet. Power is not read because this demo does not report it on this board.</p>
</div>

<div class="blk" id="what">
<h2>What the three settings are</h2>
<p><b>The chip is 2 transmit by 3 receive in every IWRL part.</b> The data sheets for the IWRL6432, the IWRL6432 WCSP, the IWRL6432AOP and the IWRL1432 (76 to 81 GHz) all list 3 receivers and 2 transmitters, read 2026-10-10. The AOP part can use one transmitter at a time. No part in this family has a single antenna pair, so the 1x1 row is this chip with one transmitter and one receiver switched on.</p>
<p><b>What the settings do.</b> More receivers and transmitters give the radar more virtual antennas: 2x3 gives 6, 2x2 gives 4, 1x1 gives 1. That buys angle, meaning where the person is across the field, and a stronger combined signal. One antenna pair tells you something moved and roughly how far. It cannot tell you where sideways.</p>
<p><b>Rates.</b> {esc(rates) if rates else "Frame rate per setting is not measured yet."}</p>
</div>

<div class="blk" id="notes">
<h2>Notes from the runs</h2>
<ul>
<li>The demo takes one configuration start per boot. Each setting costs one press of the reset button, and 1x1 is refused outright.</li>
<li>The first 2x2 walk-in started with the person already in view at 8 m, so no arrival distance exists for 2x2. The 2x3 walk started outside the room behind a closed door.</li>
<li>The demo holds a presence flag for about 11 to 14 s after the person leaves. Clear delays include that hold and the walk out.</li>
<li>Each cell is one pass. A one or two second difference between settings is within the person's own timing.</li>
<li>A Wi-Fi presence model on a second board (TI's CSI model) reported present in every result, including the empty-room windows, so it gave no clear time in this room; the radar cleared.</li>
</ul>
</div>

<div class="blk" id="accept">
<h2>What the demo accepted</h2>
{accept_html}
</div>

<div class="blk" id="how">
<h2>How this was measured</h2>
{ui.fold("The test, step by step", "<p>The tool <code>iwrl6432boost/eval/radar_eval.py</code> sends the stock presence configuration over the radar's serial port with one line changed (<code>channelCfg</code>: receive mask, transmit mask), plus <code>lowPowerCfg 0</code> so the demo will stop and restart between settings. It then reads the radar's frames with timestamps and works out the columns above. The empty-room window runs with nobody in the room; the other passes are voice-cued. The stock configuration is put back at the end.</p><p>The demo reports presence by four boxes on the floor (two near, two far, split at 3.5 m) and a range profile, not a distance for each person, so distance comes from where the person stood when presence first showed. A reflector pass and a motion-only demo pass are not in this version.</p>")}
{ui.fold("Run it yourself", "<p><code>python3 iwrl6432boost/eval/radar_eval.py accept</code> tries each setting. <code>... empty --seconds 300</code> runs the empty room. <code>... run --passes walk,sit2,sit4</code> posts each pass as a request and waits for you. <code>python3 iwrl6432boost/eval/render_eval.py</code> rebuilds this page from the results files.</p>")}
</div>
"""
    body += rb.folded_glossary(body)
    return rb.page("Radar evaluation", "Antenna setting comparison for the TI IWRL6432 60 GHz radar: detection distance, delay, false alarms for 1x1, 2x2 and 2x3 antenna settings.", body, short="Radar evaluation")

if __name__ == "__main__":
    t = render()
    if "--check" in sys.argv:
        if not os.path.exists(OUT) or open(OUT).read() != t: print("DRIFT: site/radar-eval.html out of date"); sys.exit(1)
        print("radar-eval OK")
    else:
        open(OUT, "w").write(t); print("wrote", OUT)
