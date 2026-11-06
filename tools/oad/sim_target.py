#!/usr/bin/env python3
"""Test oad_client.py without a board: a stand-in for the CC2340R5 OAD target behind a fake bleak.

  sim_target.py <image.bin> [att_mtu] [mode]   default MTU 247; try 185 to see the client refuse
        mode: normal (default) | dup (a second board with the same name: client must refuse, exit 1)
              | address (dup, but the run picks one board with --address SIM) | enablefail (Enable Image
              answers VALIDATION_ERR: client must exit nonzero) | impostor (after the reset only another
              address shows the new name: verification must fail)

The stand-in follows the state machine in the F3 SDK's oad_profile.c (idle, config, download,
complete), stores each block in a 0x3B000-byte slot, and on Enable Image "reboots" into the new
name and version if the slot holds a valid, marked image. It proves the client's logic, not the radio."""
import asyncio, os, struct, sys, types
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oad_client as oc

STATE = {"name": "FRL OAD 1", "ver": (1, 0, 0), "flash": bytearray(0x3B000), "mtu": int(sys.argv[2]) if len(sys.argv) > 2 else 247,
         "mode": sys.argv[3] if len(sys.argv) > 3 else "normal"}
class Dev:
    def __init__(s, name, address="SIM"): s.name, s.address = name, address
class Adv:
    def __init__(s, n, rssi=-60): s.local_name, s.rssi = n, rssi
class Svc: pass
class Services:
    def get_service(s, u): return Svc() if u == oc.SVC else None

class Target:
    """Mirror of oad_profile.c: idle -> config -> download -> complete."""
    def __init__(s, notify): s.notify, s.state, s.blk, s.total, s.blkSize = notify, "IDLE", 0, None, 240
    def ctrl(s, d):
        op = d[0]
        if op == 0x01: return s.notify(oc.CONTROL, bytes([1]) + struct.pack("<H", s.blkSize))
        if op == 0x07:
            ma, mi, rv = STATE["ver"]; return s.notify(oc.CONTROL, bytes([7, ma, mi, rv >> 8, rv & 255, 0, 0]))
        if op == 0x03 and s.state == "CONFIG":
            s.state = "DOWNLOAD"; return s.notify(oc.CONTROL, bytes([0x12, 0]) + struct.pack("<I", 0))
        if op == 0x04 and s.state == "COMPLETE" and STATE["mode"] == "enablefail":
            return s.notify(oc.CONTROL, bytes([4, 1]))
        if op == 0x04 and s.state == "COMPLETE":
            s.notify(oc.CONTROL, bytes([4, 0])); asyncio.get_event_loop().call_later(0.2, s.reset); return
        s.notify(oc.CONTROL, bytes([op, 5 if s.state == "IDLE" else 4]))
    def identify(s, d):
        if struct.unpack_from("<I", d, 0)[0] != oc.IMAGE_MAGIC: return s.notify(oc.IDENTIFY, bytes([10]))
        ln = struct.unpack_from("<I", d, 12)[0]
        per = s.blkSize - 4; s.total = -(-ln // per); s.last = ln % per; s.len = ln
        s.state = "CONFIG"; s.notify(oc.IDENTIFY, bytes([0]))
    def block(s, d):
        n = struct.unpack_from("<I", d, 0)[0]; per = s.blkSize - 4
        exp = (s.last + 4) if (n == s.total - 1 and s.last) else s.blkSize
        if n != s.blk or len(d) != exp:
            s.state = "IDLE"; return s.notify(oc.CONTROL, bytes([0x12, 3]) + struct.pack("<I", 0))
        STATE["flash"][n * per:n * per + len(d) - 4] = d[4:]; s.blk += 1
        if s.blk == s.total:
            s.state = "COMPLETE"; return s.notify(oc.CONTROL, bytes([0x12, 14]) + struct.pack("<I", s.blk))
        s.notify(oc.CONTROL, bytes([0x12, 0]) + struct.pack("<I", s.blk))
    def reset(s):
        img = bytes(STATE["flash"])
        info, prob = oc.parse_image(img)
        print("SIM: MCUboot sees secondary:", info.get("version"), "problems:", prob, "trailer", info.get("trailer_magic"))
        if not prob and info["trailer_magic"]:
            STATE["ver"] = tuple(int(x) for x in info["version"].split("+")[0].split("."))
            STATE["name"] = "FRL OAD 2"
        s.disc()

class FakeClient:
    def __init__(s, dev, disconnected_callback=None, timeout=20): s.cbs, s.dcb = {}, disconnected_callback; s.services = Services()
    async def __aenter__(s):
        s.mtu_size = STATE["mtu"]; s.t = Target(lambda u, d: s.cbs[u](None, bytearray(d)) if u in s.cbs else None)
        s.t.disc = lambda: s.dcb and s.dcb(s); return s
    async def __aexit__(s, *a): pass
    async def start_notify(s, u, cb): s.cbs[u] = cb
    async def write_gatt_char(s, u, data, response=None):
        await asyncio.sleep(0.0005)
        {oc.CONTROL: s.t.ctrl, oc.IDENTIFY: s.t.identify, oc.BLOCK: s.t.block}[u](bytes(data))

class FakeScanner:
    """Async context manager like bleak.BleakScanner(detection_callback=...): reports the nearby boards."""
    def __init__(s, detection_callback=None): s.cb = detection_callback
    def boards(s):
        out = [(Dev(STATE["name"], "SIM"), Adv(STATE["name"], -50))]
        if STATE["mode"] in ("dup", "address"):
            out.append((Dev(STATE["name"], "SIM2"), Adv(STATE["name"], -70)))
        if STATE["mode"] == "impostor" and STATE["name"] == "FRL OAD 2":
            out = [(Dev("FRL OAD 2", "OTHER"), Adv("FRL OAD 2", -40))]
        return out
    async def __aenter__(s):
        asyncio.get_event_loop().call_later(0.05, lambda: [s.cb(d, a) for d, a in s.boards()])
        return s
    async def __aexit__(s, *a): pass

bleak = types.ModuleType("bleak")
bleak.BleakClient = FakeClient
bleak.BleakScanner = FakeScanner
sys.modules["bleak"] = bleak
args = types.SimpleNamespace(image=sys.argv[1], slot_size=0x3B000, keys_c=None, name="FRL OAD 1", address="SIM" if STATE["mode"] == "address" else None, expect_name="FRL OAD 2",
                             scan_timeout=2, block_timeout=3, reboot_timeout=2, no_response=False, json=None)
if STATE["mode"] == "impostor": args.reboot_timeout = 1
asyncio.run(oc.run(args))
print("SIM: flash matches file:", bytes(STATE["flash"]) == open(sys.argv[1], "rb").read())
