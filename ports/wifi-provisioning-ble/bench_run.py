#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Time Bluetooth provisioning on an LP-EM-CC35X1 running the chained demo: credentials written over BLE, Wi-Fi
associated, IP address, first MQTT message on the broker. All stamps on this computer's clock.

  python3 bench_run.py --mode provision --runs 5   # each run: wait for advertising, provision, time, forget-wifi
  python3 bench_run.py --mode reset                # reset with stored credentials: rejoin without Bluetooth
  python3 bench_run.py --mode wrongpw              # wrong password: the board must report a failure over BLE

Settings come from the environment: FRL_SSID, FRL_WIFI_ENV (a file with WIFI_PASSWORD=...), FRL_BROKER (host),
FRL_BROKER_PORT, FRL_TOKEN (the CHAINED_CMD_TOKEN), FRL_PROBE (probe serial), FRL_LOG (raw console log file).
The network name in the console log is replaced by <house network>; the password is never written anywhere.
Run it under the probe lock: tools/frl/lock.py exec --serial <probe> -- python3 bench_run.py ...
"""
import argparse, asyncio, json, os, re, subprocess, sys, threading, time
import serial
import paho.mqtt.client as mqtt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from provision import provision

E = os.environ
SSID, PROBE = E["FRL_SSID"], E.get("FRL_PROBE", "E10000FQ")
PW = [l.split("=", 1)[1].strip().strip("'\"") for l in open(E["FRL_WIFI_ENV"]) if l.startswith("WIFI_PASSWORD=")][0]
RESET = "/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset"
lines, msgs, lock = [], [], threading.Lock()   # (t, text)


def scrub(s):
    s = s.replace(SSID, "<house network>")
    return s.replace(PW, "<password>") if PW else s


def serial_reader(stop):
    s = serial.Serial(f"/dev/cu.usbmodem{PROBE}1", 115200, timeout=0.05)
    buf = b""
    raw = open(E.get("FRL_LOG", os.devnull), "a")
    while not stop.is_set():
        buf += s.read(4096)
        while b"\n" in buf:
            ln, buf = buf.split(b"\n", 1)
            txt = scrub(ln.decode("utf-8", "replace").rstrip("\r"))
            t = time.perf_counter()
            with lock:
                lines.append((t, txt))
            raw.write(f"{t:.3f} {txt}\n"); raw.flush()


def on_msg(_c, _u, m):
    with lock:
        msgs.append((time.perf_counter(), m.topic, scrub(m.payload.decode("utf-8", "replace"))))


def wait_line(pat, after, timeout):
    rx, end = re.compile(pat), time.perf_counter() + timeout
    while time.perf_counter() < end:
        with lock:
            for t, x in lines:
                if t >= after and rx.search(x):
                    return t, x
        time.sleep(0.02)
    return None, None


def wait_msg(after, timeout, pat="."):
    rx, end = re.compile(pat), time.perf_counter() + timeout
    while time.perf_counter() < end:
        with lock:
            for t, tp, p in msgs:
                if t >= after and rx.search(tp + " " + p):
                    return t, tp, p
        time.sleep(0.02)
    return None, None, None


def reset():
    t = time.perf_counter()
    subprocess.run([RESET, "-s", PROBE], capture_output=True)
    return t


ASSOC = r"(?i)\[WIFI\] event|associat|connected to|CONNECTED"


def one_provision(cli, password, expect_ok=True):
    t0 = time.perf_counter()
    ta, _ = wait_line(r"\[PROV\] advertising", t0 - 60, 60)
    r, t = asyncio.run(provision(SSID, password, "wpa2", timeout=45))
    out = {"result": r}
    if "requested" not in t:
        return out
    tr = t["requested"]
    out["ble_write_s"] = round(t["written"] - t["connected"], 3)
    out["ble_find_connect_s"] = round(t["connected"] - t["start"], 3)
    if "note3" in t:
        out["fail_notify_s"] = round(t["note3"] - tr, 2)
    if "note2" in t:
        out["ok_notify_s"] = round(t["note2"] - tr, 2)
    if not expect_ok:
        return out
    tas, xa = wait_line(ASSOC, tr, 30)
    tip, _ = wait_line(r"\[WIFI\] connected, addr=", tr, 30)
    tm, tp, p = wait_msg(tr, 40)
    out.update(assoc_s=tas and round(tas - tr, 2), assoc_line=xa, ip_s=tip and round(tip - tr, 2),
               mqtt_s=tm and round(tm - tr, 2), mqtt_first=f"{tp} {p}" if tp else None)
    tsv, _ = wait_line(r"\[PROV\] joined; credentials saved", tr, 30)
    out["saved"] = tsv is not None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["provision", "reset", "wrongpw", "forget"], required=True)
    ap.add_argument("--runs", type=int, default=1)
    ap.add_argument("--fresh", action="store_true", help="reset the board first")
    a = ap.parse_args()
    stop = threading.Event()
    threading.Thread(target=serial_reader, args=(stop,), daemon=True).start()
    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cli.on_message = on_msg
    cli.connect(E["FRL_BROKER"], int(E.get("FRL_BROKER_PORT", 1884)))
    cli.subscribe("frl/cc35x1/#")
    cli.loop_start()
    time.sleep(0.5)
    res = []
    for i in range(a.runs):
        if a.mode == "provision":
            if a.fresh or i == 0:
                reset()
            r = one_provision(cli, PW)
            res.append(r); print(json.dumps(r), flush=True)
            if i < a.runs - 1 or os.environ.get("FRL_FORGET_LAST"):
                tf = time.perf_counter()
                cli.publish("frl/cc35x1/cmd", f"token={E['FRL_TOKEN']} forget-wifi")
                wait_line(r"\[PROV\] advertising", tf, 60)
        elif a.mode == "wrongpw":
            if a.fresh:
                reset()
            r = one_provision(cli, PW + "x", expect_ok=False)
            res.append(r); print(json.dumps(r), flush=True)
        elif a.mode == "forget":
            tf = time.perf_counter()
            cli.publish("frl/cc35x1/cmd", f"token={E['FRL_TOKEN']} forget-wifi")
            t, _ = wait_line(r"\[PROV\] advertising", tf, 60)
            print(json.dumps({"forget_to_advertising_s": t and round(t - tf, 2)}), flush=True)
        else:
            tr = reset()
            tip, _ = wait_line(r"\[WIFI\] connected, addr=", tr, 60)
            tm, tp, p = wait_msg(tr, 60)
            tadv, _ = wait_line(r"\[PROV\]", tr, 0.1)
            r = {"reset_ip_s": tip and round(tip - tr, 2), "reset_mqtt_s": tm and round(tm - tr, 2),
                 "prov_lines": tadv is not None, "mqtt_first": f"{tp} {p}" if tp else None}
            res.append(r); print(json.dumps(r), flush=True)
    time.sleep(1)
    stop.set()
    cli.loop_stop()


if __name__ == "__main__":
    main()
