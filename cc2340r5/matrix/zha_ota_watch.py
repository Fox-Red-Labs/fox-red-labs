#!/usr/bin/env python3
"""Log a ZHA over-the-air update every few seconds: the update entity's progress (REST) and the device's
link readings as Home Assistant reports them (LQI, RSSI, last seen, short address; websocket).
Usage: zha_ota_watch.py <ieee> <update entity id> <minutes> <out.csv> [interval s]"""
import asyncio, csv, sys, time, datetime, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../tools"))
from zha_ws import call
import ha

ieee, ent, minutes, out = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
iv = float(sys.argv[5]) if len(sys.argv) > 5 else 5
f = open(out, "a", newline=""); w = csv.writer(f)
if f.tell() == 0:
    w.writerow(["time", "state", "in_progress", "update_percentage", "installed", "latest", "available", "lqi", "rssi", "last_seen", "nwk"])
t_end = time.time() + minutes * 60
while time.time() < t_end:
    try:
        a = ha.req("GET", f"/api/states/{ent}")[1]; at = a.get("attributes", {})
        e = [a.get("state"), at.get("in_progress"), at.get("update_percentage"), at.get("installed_version"), at.get("latest_version")]
    except Exception as ex:
        e = [f"err {ex}", "", "", "", ""]
    try:
        d = asyncio.run(call([{"type": "zha/device", "ieee": ieee}]))[0].get("result") or {}
    except Exception as ex:
        d = {"available": f"err {ex}"}
    row = [datetime.datetime.now().strftime("%H:%M:%S")] + e + [d.get("available"), d.get("lqi"), d.get("rssi"), d.get("last_seen"), d.get("nwk")]
    w.writerow(row); f.flush(); print(",".join(map(str, row)), flush=True)
    time.sleep(iv)
