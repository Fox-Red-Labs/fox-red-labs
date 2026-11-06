#!/usr/bin/env python3
"""Run TI's Wi-Fi CSI presence example on an LP-EM-CC35X1 from a Mac, with no Edge AI Studio.

Written 2026-10-09 BEFORE the board was on the desk. The DAP parser is checked against
synthetic frames (--selftest); the serial conversation is NOT yet run on hardware.

What it does
  1. Opens the board's serial port at 921600 baud.
  2. Types TI's console commands (README of edgeai_wifi_csi_occupancy_detection_live_preview):
     wlan_start, wlan_sta_role_up, wlan_connect (password unquoted), csi_enable, csi_config -s 1 -p 7,
     csi_set_mac, csi_get_results. The network name, password and access-point MAC come from the
     environment (FRL_WIFI_SSID, FRL_WIFI_PSK, FRL_AP_BSSID) or a hidden prompt. They are never
     compiled into the image, never written to a file, and are scrubbed from the saved log.
  3. Waits for "DAP streaming ready", sends the DAP start-streaming frame, and decodes the
     board's inference frames (class, probability of presence), one per ~2 s window.
  4. Runs a timed protocol (empty room, one person still, one person moving, empty again),
     cues the person with the Mac's `say`, scores each phase, writes a CSV and a summary.

Needs pyserial:  python3 -m venv ~/.venvs/csi && ~/.venvs/csi/bin/pip install pyserial
Output goes under out/cc35x1/csi-run/ (not the repo). Do not commit it.
"""
import argparse, csv, getpass, json, os, re, struct, subprocess, sys, time

BAUD = 921600
START, END = 0xED, 0x9E
CMD_START_STREAMING = 0x08
CMD_DATA = 0x10
DATA_INFERENCING_VALUE = 0x05
WINDOW_S = 2.0          # one inference per 2 s CSI window (TI README, How It Works)
SETTLE_S = 20.0         # seconds after each cue not scored (person moves, soft average of 3 windows = 6 s)

# (label for scoring: 0 empty / 1 presence, name, minutes, cue spoken and printed)
PROTOCOL = [
    (0, "empty_1",  5, "Leave the room now and close the door. Stay out until you hear the next cue."),
    (1, "still",    5, "Come in, sit in the chair and stay still. Breathe normally. Phone down."),
    (1, "moving",   5, "Stand up and walk around the room, normal pace, for the whole time."),
    (0, "empty_2",  5, "Leave the room again and close the door."),
]


def build_frame(cmd, payload=b""):
    assert len(payload) < 0x80
    return bytes([START, cmd, len(payload)]) + payload + bytes([END])


def parse_frames(buf):
    """Yield (cmd, payload) for each complete ED .. 9E frame in buf; return the unparsed tail."""
    out, i = [], 0
    while True:
        j = buf.find(bytes([START]), i)
        if j < 0 or len(buf) - j < 4:
            return out, buf[j:] if j >= 0 else b""
        cmd, b0 = buf[j + 1], buf[j + 2]
        if not (cmd <= 0x0E or 0x10 <= cmd <= 0x16):      # not a DAP command: a stray 0xED in console text
            i = j + 1
            continue
        if b0 < 0x80:
            n, hdr = b0, 3
        elif (b0 & 0xC0) == 0x80:
            if len(buf) - j < 5:
                return out, buf[j:]
            n, hdr = ((b0 & 0x3F) << 8) | buf[j + 3], 4
        else:
            if len(buf) - j < 6:
                return out, buf[j:]
            n, hdr = ((b0 & 0x3F) << 16) | (buf[j + 3] << 8) | buf[j + 4], 5
        if len(buf) - j < hdr + n + 1:
            return out, buf[j:]
        if buf[j + hdr + n] != END:
            i = j + 1            # false start byte inside text; keep scanning
            continue
        out.append((cmd, buf[j + hdr:j + hdr + n]))
        i = j + hdr + n + 1


def decode_inference(cmd, payload):
    """TI's Pres_InferenceResult: int8 class, then float32 P(presence). Returns (class, p1) or None."""
    if cmd != (CMD_DATA | DATA_INFERENCING_VALUE) or len(payload) != 5:
        return None
    cls = struct.unpack("b", payload[:1])[0]
    p1 = struct.unpack(">f", payload[1:5])[0]       # DAP_swapEndianness: big-endian on the wire
    return cls, p1


def selftest():
    good = build_frame(CMD_DATA | DATA_INFERENCING_VALUE, struct.pack("b", 1) + struct.pack(">f", 0.875))
    noise = b"DAP streaming ready\r\n\xed\xffjunk"
    frames, tail = parse_frames(noise + good + good[:4])
    assert [decode_inference(c, p) for c, p in frames] == [(1, 0.875)], frames
    assert tail == good[:4], tail
    err = build_frame(0x00, b"\x00Not ready")
    frames, _ = parse_frames(err)
    assert frames == [(0, b"\x00Not ready")] and decode_inference(*frames[0]) is None
    print("selftest ok")


class Scrub:
    """Remove the secrets from anything written to disk."""
    def __init__(self, *secrets):
        self.s = [x for x in secrets if x]

    def __call__(self, text):
        for x in self.s:
            text = text.replace(x, "[redacted]")
        return text


def freerun(ser, say):
    """Log every inference frame with the host clock (epoch seconds plus ISO, same clock as the radar tool) until Ctrl-C."""
    import datetime
    d = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, datetime.date.today().isoformat() + "-freerun.csv")
    new = not os.path.exists(path)
    n, rx = 0, b""
    print("free-running: logging to", path, "(Ctrl-C to stop)", flush=True)
    with open(path, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["t", "iso", "cls", "p1"])
        try:
            while True:
                rx += ser.read(256)
                frames, rx = parse_frames(rx)
                for cmd, payload in frames:
                    r = decode_inference(cmd, payload)
                    if r is None:
                        continue
                    t = time.time()
                    w.writerow(["%.3f" % t, datetime.datetime.fromtimestamp(t).isoformat(timespec="milliseconds"), r[0], round(r[1], 4)])
                    f.flush(); n += 1
                    if n % 15 == 0:
                        print("%s  %d results, last class %d p=%.2f" % (time.strftime("%H:%M:%S"), n, r[0], r[1]), flush=True)
        except KeyboardInterrupt:
            print("\nstopped,", n, "results in", path)
    say("freerun stopped, %d results" % n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="/dev/cu.usbmodem<probe serial>1 (the application port, not the debug port)")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--minutes", type=float, default=None, help="override every phase length (for a short rehearsal)")
    ap.add_argument("--note", default="", help="free text for the summary: AP model, distances, rooms (no password)")
    ap.add_argument("--ssid", help="network name (else FRL_WIFI_SSID or a hidden prompt)")
    ap.add_argument("--keychain", action="store_true",
                    help="read the password from this Mac's Keychain (security find-generic-password; macOS asks you to Allow once). Run this yourself, not an agent.")
    ap.add_argument("--bssid", default="auto", help="access point MAC, or 'auto' to take it from the board's own join message")
    ap.add_argument("--resume", action="store_true",
                    help="the board is already joined and has printed 'DAP inferencing ready': skip the setup, send start streaming, run the protocol (no login needed)")
    ap.add_argument("--freerun", action="store_true",
                    help="after the same login and start-streaming, just log every result with a host timestamp to cc35x1/csi/results/<date>-freerun.csv until Ctrl-C: no phases, no cues. For side-by-side runs with another sensor.")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    import serial  # pyserial
    if not a.port:
        sys.exit("--port is required")
    ssid = psk = None; bssid = "auto"
    if not a.resume:
        ssid = a.ssid or os.environ.get("FRL_WIFI_SSID") or getpass.getpass("network name (hidden): ")
        if a.keychain:
            r = subprocess.run(["security", "find-generic-password", "-wa", ssid], capture_output=True, text=True)
            psk = r.stdout.strip()
            if r.returncode or not psk:
                sys.exit("Keychain gave no password for that network (did you click Allow?)")
        else:
            psk = os.environ.get("FRL_WIFI_PSK") or getpass.getpass("password (hidden): ")
        bssid = a.bssid if a.bssid != "auto" else (os.environ.get("FRL_AP_BSSID") or "auto")
        if bssid != "auto" and not re.fullmatch(r"([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", bssid):
            sys.exit("bad MAC")
    scrub = Scrub(ssid, psk, bssid if bssid != "auto" else None)
    os.umask(0o077)  # everything this run writes is readable by this account only
    out = os.path.expanduser("out/cc35x1/csi-run/" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, mode=0o700, exist_ok=True)
    os.chmod(out, 0o700)
    log = os.fdopen(os.open(out + "/console.log", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w")

    ser = serial.Serial(a.port, BAUD, timeout=0.2)
    time.sleep(1.0)

    def say(line):
        log.write(scrub(line) + "\n"); log.flush()

    def send(cmd, wait_for=None, timeout=15.0, secret=False):
        # Device output is logged only as whole lines, each scrubbed as a whole (a secret split
        # across two serial reads is never written). For a command carrying credentials nothing
        # the device says back is logged, since the device may echo the command in some other form.
        say(">> " + ("[command with credentials]" if secret else cmd))
        # One character at a time: at 921600 baud a line sent in one burst is cut and garbled after
        # about 50 characters (console input overrun, measured 2026-10-09: 50 of 72 kept).
        for ch in cmd.encode() + b"\r\n":
            ser.write(bytes([ch])); time.sleep(0.004)
        t0, buf, pending = time.time(), "", ""

        def flush_lines(final=False):
            nonlocal pending
            if secret:
                pending = ""
                return
            *lines, pending = pending.split("\n")
            if final and pending:
                lines.append(pending); pending = ""
            for ln in lines:
                if ln.strip():
                    say(ln.rstrip("\r"))

        last = time.time()
        while time.time() - t0 < timeout:
            chunk = ser.read(512).decode("latin-1")
            buf += chunk
            pending += chunk
            if chunk:
                last = time.time()
            flush_lines()
            # With no pattern to wait for, return once the console has shown its prompt and been quiet
            # for 2 s. wlan_sta_role_up keeps the console busy for over a second after it echoes; a line
            # typed during that is cut at about 39 characters (measured 2026-10-09).
            quiet = (not wait_for) and "user:" in buf and time.time() - last > 2.0
            if (wait_for and re.search(wait_for, buf)) or quiet:
                flush_lines(final=True)
                if secret:
                    say("[device reply to the credentialed command withheld]")
                return buf
        flush_lines(final=True)
        if wait_for:
            tail = scrub("\n".join(l for l in buf.splitlines() if l.strip())[-600:])
            sys.exit("timed out waiting for %r after %r\nlast lines from the board (secrets scrubbed):\n%s"
                     % (wait_for, "[command]" if secret else cmd, tail))
        return buf

    join_s = None
    if a.resume:
        say("resume: board already joined and ready, sending DAP start streaming")
    else:
        t_join = time.time()
        send("wlan_start")
        send("wlan_sta_role_up")
        # The password goes unquoted: this console's parser rejects a quoted one with
        # "Parser expects security type parameter" (seen 2026-10-09). The name may hold spaces and is quoted.
        joined = send('wlan_connect -s "%s" -t WPA2 -p %s' % (ssid, psk), wait_for=r"(?i)ip.{0,30}\d+\.\d+\.\d+\.\d+", timeout=60, secret=True)
        join_s = time.time() - t_join
        if bssid == "auto":
            # The board prints "STA Connected to the AP: <name> , BSSID: c4:41:1e:82:c:f4" (octets not zero-padded).
            m = re.search(r"(?i)BSSID:\s*((?:[0-9a-f]{1,2}:){5}[0-9a-f]{1,2})", joined)
            if not m:
                sys.exit("joined, but the board did not print the access point MAC; pass --bssid")
            bssid = ":".join("%02x" % int(x, 16) for x in m.group(1).split(":"))
            scrub.s.append(bssid)
            say("access point MAC taken from the board's join message")
        send("csi_enable")
        send("csi_config -s 1 -p 7")
        send("csi_set_mac -a 1 -m " + bssid, secret=True)
        # TI's README says "DAP streaming ready"; plugin 1.20 prints "[CSI] DAP inferencing ready -- console input
        # disabled, connect Edge AI Studio now" (seen 2026-10-09).
        send("csi_get_results", wait_for=r"DAP (streaming|inferencing) ready", timeout=30)
    ser.write(build_frame(CMD_START_STREAMING))
    say(">> DAP start streaming")

    if a.freerun:
        return freerun(ser, say)
    rows, rx = [], b""
    t_start = time.time()
    first = None
    phase_times = []
    for label, name, mins, cue in PROTOCOL:
        mins = a.minutes if a.minutes is not None else mins
        print("\n== %s (%.1f min): %s" % (name, mins, cue))
        subprocess.run(["say", cue], check=False)
        t_ph = time.time()
        phase_times.append((name, label, t_ph - t_start))
        while time.time() - t_ph < mins * 60:
            rx += ser.read(256)
            frames, rx = parse_frames(rx)
            for cmd, payload in frames:
                r = decode_inference(cmd, payload)
                if r is None:
                    say("frame cmd=0x%02x %r" % (cmd, payload[:40]))
                    continue
                if first is None:
                    first = time.time() - t_start
                rows.append(dict(t=round(time.time() - t_start, 2), phase=name, label=label,
                                 settle=int(time.time() - t_ph < SETTLE_S), cls=r[0], p1=round(r[1], 4)))
    subprocess.run(["say", "Done. Thank you."], check=False)
    with os.fdopen(os.open(out + "/windows.csv", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["t", "phase", "label", "settle", "cls", "p1"])
        w.writeheader(); w.writerows(rows)

    summary = dict(note=scrub(a.note), join_s=round(join_s, 2) if join_s is not None else None, first_inference_s_after_stream_start=first,
                   windows_total=len(rows), phases={})
    for _, name, _, _ in PROTOCOL:
        sc = [r for r in rows if r["phase"] == name and not r["settle"]]
        if sc:
            ok = sum(1 for r in sc if r["cls"] == r["label"])
            summary["phases"][name] = dict(scored=len(sc), correct=ok, accuracy=round(ok / len(sc), 4),
                                           mean_p1=round(sum(r["p1"] for r in sc) / len(sc), 4))
    allsc = [r for r in rows if not r["settle"]]
    if allsc:
        summary["overall_accuracy"] = round(sum(1 for r in allsc if r["cls"] == r["label"]) / len(allsc), 4)
    with os.fdopen(os.open(out + "/summary.json", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as f:
        f.write(json.dumps(summary, indent=1))
    print(json.dumps(summary, indent=1)); print("saved in", out)


if __name__ == "__main__":
    main()
