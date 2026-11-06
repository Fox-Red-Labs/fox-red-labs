#!/usr/bin/env python3
"""The Zigbee light's keep-it-running checks against a real Home Assistant (cc2340r5/zigbee/README.md).

Run from the repository root, holding the probe lock:
  python3 tools/frl/lock.py exec --serial LS470GTU -- python3 cc2340r5/zigbee/home-assistant/keep_running.py <cmd>

  join HEX              remove the board's IEEE from ZHA, open joining, flash HEX with DSLite, wait for its new switch
  toggles ENTITY [N]    N switch commands (default 20): success count and latency, as zigbee/toggle-bench.txt measured
  reset ENTITY          reset the chip through the debugger (DSLite reset 0: a reboot, not a power cut), then time
                        until a command lands again, then 20 more
  automation ENTITY     create a temporary automation (a time trigger 90 s ahead turns the switch on), check it fired,
                        then delete it
  watch ENTITY SECONDS  after a power cut done by hand: time until a command lands again, then 20 more

Home Assistant's address and token come from tools/ha.py (read at run time, never printed). Results print as JSON.
"""
import json, statistics, subprocess, sys, time, datetime as dt, pathlib
from zoneinfo import ZoneInfo

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import ha  # noqa: E402

IEEE = "00:12:4b:xx:xx:xx:xx:xx"          # this LP-EM-CC2340R5's chip, as ZHA lists it
DSLITE = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
CCXML = ROOT / "cc2340r5" / "CC2340R5.ccxml"


def state(entity):
    return ha.req("GET", f"/api/states/{entity}")[1]


def switches():
    return {s["entity_id"] for s in ha.req("GET", "/api/states")[1] if s["entity_id"].startswith("switch.")}


def toggle_once(entity, want, timeout=10.0):
    """One command; ms from the call to the state reading `want`, or None if it never did."""
    t0 = time.time()
    try:
        ha.req("POST", f"/api/services/switch/turn_{want}", {"entity_id": entity})
    except Exception:
        return None
    while time.time() - t0 < timeout:
        if state(entity)["state"] == want:
            return round((time.time() - t0) * 1000)
        time.sleep(0.05)
    return None


def toggles(entity, n=20):
    cur = state(entity)["state"]
    times = []
    for i in range(n):
        want = "off" if cur == "on" else "on"
        ms = toggle_once(entity, want)
        times.append(ms)
        cur = want if ms is not None else cur
        time.sleep(1.0)
    ok = [t for t in times if t is not None]
    return {"commands": n, "landed": len(ok), "median_ms": statistics.median(ok) if ok else None,
            "min_ms": min(ok) if ok else None, "max_ms": max(ok) if ok else None, "all_ms": times}


def back_after(entity, t0, limit=600):
    """Seconds from t0 until one command lands (state follows within 10 s)."""
    while time.time() - t0 < limit:
        want = "off" if state(entity)["state"] == "on" else "on"
        if toggle_once(entity, want, timeout=10) is not None:
            return round(time.time() - t0, 1)
        time.sleep(2)
    return None


def join(hexfile):
    before = switches()
    try:
        ha.req("POST", "/api/services/zha/remove", {"ieee": IEEE})
    except Exception as e:                     # not paired: nothing to remove
        print("remove:", type(e).__name__)
    time.sleep(6)
    t_permit = time.time()
    ha.req("POST", "/api/services/zha/permit", {"duration": 240})
    fl = subprocess.run([DSLITE, "flash", "-c", str(CCXML), "-e", "-f", "-u", str(hexfile)], capture_output=True, text=True)
    t_flash = time.time()
    if fl.returncode:
        return {"error": "flash failed", "tail": fl.stdout[-400:] + fl.stderr[-400:]}
    entity = None
    while time.time() - t_permit < 300:
        new = switches() - before
        if new:
            entity = sorted(new)[0]
            break
        time.sleep(0.5)
    t_ent = time.time()
    return {"entity": entity, "flash_s": round(t_flash - t_permit, 1), "switch_after_flash_s": round(t_ent - t_flash, 1) if entity else None,
            "friendly_name": state(entity)["attributes"].get("friendly_name") if entity else None}


def reset(entity):
    t0 = time.time()
    r = subprocess.run([DSLITE, "flash", "-c", str(CCXML), "-r", "0"], capture_output=True, text=True)
    t_reset = time.time()
    if r.returncode:
        return {"error": "reset failed", "tail": r.stdout[-400:]}
    back = back_after(entity, t_reset)
    return {"reset": "DSLite reset 0 (device reset, debugger connect and disconnect)", "reset_s": round(t_reset - t0, 1),
            "first_command_landed_after_s": back, "then": toggles(entity) if back is not None else None}


def automation(entity):
    tz = ZoneInfo(ha.req("GET", "/api/config")[1]["time_zone"])
    if toggle_once(entity, "off") is None and state(entity)["state"] != "off":
        return {"error": "could not turn the switch off first"}
    at = (dt.datetime.now(tz) + dt.timedelta(seconds=90)).replace(microsecond=0)
    aid = "frl_keep_running_check"
    cfg = {"id": aid, "alias": "FRL keep-running check (temporary)", "description": "Made and deleted by keep_running.py",
           "triggers": [{"trigger": "time", "at": at.strftime("%H:%M:%S")}],
           "actions": [{"action": "switch.turn_on", "target": {"entity_id": entity}}], "mode": "single"}
    ha.req("POST", f"/api/config/automation/config/{aid}", cfg)
    fired = None
    try:
        t_end = time.time() + 150
        while time.time() < t_end:
            s = state(entity)
            if s["state"] == "on":
                fired = s["last_changed"]
                break
            time.sleep(1)
    finally:
        ha.req("DELETE", f"/api/config/automation/config/{aid}")
    return {"trigger_at": at.isoformat(), "switch_on_at": fired, "fired": fired is not None, "automation_deleted": True,
            "format": "triggers/trigger: time, actions/action: switch.turn_on (the file's format)"}


def watch(entity, seconds):
    t0 = time.time()
    back = back_after(entity, t0, limit=float(seconds))
    return {"first_command_landed_after_s": back, "then": toggles(entity) if back is not None else None}


if __name__ == "__main__":
    cmd, args = sys.argv[1], sys.argv[2:]
    out = {"join": lambda: join(args[0]), "toggles": lambda: toggles(args[0], int(args[1]) if len(args) > 1 else 20),
           "reset": lambda: reset(args[0]), "automation": lambda: automation(args[0]), "watch": lambda: watch(args[0], args[1])}[cmd]()
    out["when"] = dt.datetime.now().isoformat(timespec="seconds")
    print(json.dumps(out, indent=1))
