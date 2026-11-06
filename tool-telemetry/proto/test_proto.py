#!/usr/bin/env python3
"""Checks the C codec (tt_proto.c, built for this computer) against the Python one (tt_proto.py), byte for byte, on
fixed cases and a few thousand random ones, and checks both decoders read back what the encoders wrote.
    python3 tool-telemetry/proto/test_proto.py          prints OK and the counts, or the first mismatch"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tt_proto as P  # noqa: E402

FIXED = [
    "$TS,n=42,id=UNIT-07,trig=118,oc=3,flt=0,run=5120,rpm=3566,ipk=2150,vb=12400,tc=31,st=1",
    "$TB,n=43,soc=87,cyc=212,vb=20100,tc=29",
    "$TE,n=44,ev=oc,v=8200",
    "$TE,n=45,ev=trig,v=1340",
    "$TC,n=1,cmd=run,duty=20",
    "$TS,n=46,trig=4294967295,oc=70000,tc=-40,st=2,rpm=0",
    "$TS,n=47,bogus=xyz,trig=abc,rpm=12",
]
BAD = ["", "$TS*00", "TS,n=1*00", "$TS,n=1*0G", "$TS,n=1,trig*7A", "$" + "X" * 170 + "*00"]


def build_host(tmp):
    exe = os.path.join(tmp, "tt_host")
    subprocess.check_call(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-o", exe,
                           os.path.join(HERE, "tt_host.c"), os.path.join(HERE, "tt_proto.c")])
    return exe


def with_sum(body):
    return "%s*%s" % (body, P.checksum(body[1:]))


def py_state(t):
    return "S %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d" % (
        t["id"] or "-", t["trig"], t["run"], t["oc"], t["flt"], t["rpm"], t["rpmx"], t["ipk"], t["vb"], t["tc"],
        t["st"], t["has_bat"], t["soc"], t["cyc"], t["bvb"], t["btc"])


def py_decode_text(d):
    f = P.decode_adv(d)
    if f is None:
        return "none"
    s = "F %d %d %s %s %d %d %d %d" % (f["vt"], f["seq"], f.get("name") or "-", f.get("tool") or "-",
                                       f.get("trig", 0), f.get("oc", 0), f.get("flt", 0), f.get("run", 0))
    if f["vt"] == P.VT_LEGACY:
        return s + " flags=%d soc=%d tc=%d" % (f["flags"], f["soc"], f["tc"])
    live = "rpm" in f
    s += " live=%d %d %d %d %d %d %d bat=%d" % (live, f.get("rpm", 0), f.get("rpmx", 0), f.get("ipk", 0),
                                               f.get("vb", 0), f.get("tc", 0), f.get("st", 0), "cyc" in f)
    if "cyc" in f:
        s += " %d %d %d %d" % (f["soc"], f["cyc"], f["bvb"], f["btc"])
    ev = f.get("events", [])
    s += " ev=%d" % len(ev) + "".join(" %d:%d:%d:%d" % (e["id"], e["type"], e["t"], e["v"]) for e in ev)
    if "tup" in f:
        s += " tag=%d:%d:%d:%d:%d:%d" % (f["tup"], f["link"], f["rxok"], f["rxbad"], f["fw"], f["mv"])
    pl = f.get("pulls", [])
    s += " pulls=%d" % len(pl) + "".join(" " + ":".join(str(p[k]) for k in P.PULL_KEYS) for p in pl)
    return s


def rand_line(r, n):
    typ = r.choice(["TS", "TS", "TS", "TB", "TE", "TP"])
    kv = {}
    if typ == "TS":
        for k, hi in [("trig", 2**32 - 1), ("oc", 70000), ("flt", 300), ("run", 2**31), ("rpm", 70000),
                      ("ipk", 70000), ("vb", 30000), ("tc", 200), ("st", 3)]:
            if r.random() < 0.7:
                kv[k] = r.randint(-130 if k == "tc" else 0, hi)
        if r.random() < 0.3:
            kv["id"] = "".join(r.choice("ABCDXYZ0123-") for _ in range(r.randint(1, 11)))
    elif typ == "TB":
        kv = dict(soc=r.randint(0, 120), cyc=r.randint(0, 70000), vb=r.randint(0, 30000), tc=r.randint(-50, 90))
    elif typ == "TP":
        kv = dict(dur=r.randint(-20, 700000), ravg=r.randint(0, 70000), rmax=r.randint(0, 9000), iavg=r.randint(0, 9000),
                  ipk=r.randint(0, 70000), vmin=r.randint(0, 30000), tc=r.randint(-140, 140), cls=r.randint(-1, 300))
        kv = {k: v for k, v in kv.items() if r.random() < 0.85}
    else:
        kv = dict(ev=r.choice(["trig", "oc", "flt", "boot", "nope"]), v=r.randint(0, 99999))
    keys = list(kv)
    r.shuffle(keys)
    return P.make_line(typ, n, **{k: kv[k] for k in keys}).rstrip("\n")


def main():
    r = random.Random(1017)
    with tempfile.TemporaryDirectory() as tmp:
        exe = build_host(tmp)
        cmds, wants = [], []

        def ask(cmd, want):
            """Queue a command and the lines the C harness must print for it (checked in one batch at the end)."""
            cmds.append(cmd)
            wants.extend((cmd, w) for w in want)

        tool = P.new_tool()
        checks = 0

        def feed(line):
            nonlocal tool, checks
            try:
                ln, tool2 = P.parse_line(line, tool)
                pl = ln.get("pull") or dict(dur10=0, ravg=0, rmax=0, iavg=0, ipk=0, vmin=0, tc=0, cls=255)
                want0 = "ok %s %d %d %d %s %d P %d %d %d %d %d %d %d %d" % (
                    ln["type"], ln["n"], ln["ev"], ln["v"], ln["cmd"] or "-", ln["duty"], pl["dur10"], pl["ravg"], pl["rmax"],
                    pl["iavg"], pl["ipk"], pl["vmin"], pl["tc"], pl["cls"])
                tool = tool2
            except ValueError as e:
                want0 = "err %d" % (-2 if str(e) == "checksum" else -1)
            ask("L " + line, [want0, py_state(tool)])
            checks += 1

        for body in FIXED:
            feed(with_sum(body))
        for bad in BAD:
            feed(bad)
        feed(with_sum("$TS,n=9,trig=5")[:-2] + "00")      # wrong checksum

        seq = 0
        for i in range(3000):
            feed(rand_line(r, i))
            if i % 3 == 0:
                seq = (seq + 1) & 0xFF
                link, ext = r.randint(0, 1), r.randint(0, 1)
                p = P.build_legacy(seq, tool, P.flags(tool, link, ext)).hex()
                ask("G %d %d %d" % (seq, link, ext), [p])
                assert len(bytes.fromhex(p)) == 25
                evs = [dict(id=r.randint(0, 65535), type=r.randint(1, 6), t=r.randint(0, 2**32 - 1),
                            v=r.randint(0, 65535)) for _ in range(r.randint(0, 8))]
                tag = None if r.random() < 0.2 else dict(up=r.randint(0, 2**32 - 1), link=r.randint(0, 65535),
                                                         rx_ok=r.randint(0, 65535), rx_bad=r.randint(0, 255),
                                                         fw=r.randint(0, 255), mv=r.randint(0, 65535))
                name = r.choice(["-", "Tool 1A2B", "T"])
                pls = [dict(id=r.randint(0, 65535), dur10=r.randint(0, 65535), ravg=r.randint(0, 65535), rmax=r.randint(0, 65535),
                            iavg=r.randint(0, 65535), ipk=r.randint(0, 65535), vmin=r.randint(0, 65535), tc=r.randint(-128, 127),
                            cls=r.choice([0, 1, 2, 3, 255])) for _ in range(r.randint(0, 3))]
                if len(evs) > 5:
                    evs = evs[:5]               # the tag sends at most 5 events beside 3 pulls
                evtxt = ",".join("%d:%d:%d:%d" % (e["id"], e["type"], e["t"], e["v"]) for e in evs) or "-"
                tagtxt = "-" if tag is None else "%d:%d:%d:%d:%d:%d" % (tag["up"], tag["link"], tag["rx_ok"],
                                                                        tag["rx_bad"], tag["fw"], tag["mv"])
                p2 = P.build_ext(seq, tool, evs, tag, "" if name == "-" else name, pls).hex()
                pltxt = ",".join(":".join(str(p[k]) for k in P.PULL_KEYS) for p in pls) or "-"
                ask("X %d %s %s %s %s" % (seq, name.replace(" ", "_"), evtxt, tagtxt, pltxt), [p2])
                assert len(p2) // 2 <= 230, len(p2) // 2
                for h in (p, p2):
                    ask("D " + h, [py_decode_text(bytes.fromhex(h))])
                    f = P.decode_adv(bytes.fromhex(h))
                    assert f["trig"] == (min(tool["trig"], 0xFFFFFF) if f["vt"] == P.VT_LEGACY else tool["trig"])
                    assert f["oc"] == tool["oc"] and f["run"] == tool["run"]
                if "events" in P.decode_adv(bytes.fromhex(p2)) or evs:
                    assert P.decode_adv(bytes.fromhex(p2)).get("events", []) == evs
                ask("Z", [])
                tool["rpmx"] = tool["rpm"]
                checks += 4
        # junk advertising data never crashes either decoder and both agree
        for i in range(2000):
            junk = bytes(r.randint(0, 255) for _ in range(r.randint(0, 60)))
            if r.random() < 0.5:
                junk = bytes([r.randint(3, 40), 0xFF, 0xFF, 0xFF, 0x46, 0x52, r.choice([0x11, 0x12])]) + junk
            ask("D " + (junk.hex() or "00"), [py_decode_text(junk or b"\x00")])
            checks += 1
        got = subprocess.run([exe], input="\n".join(cmds) + "\n", capture_output=True, text=True,
                             timeout=120).stdout.splitlines()
        assert len(got) == len(wants), "C printed %d lines, expected %d" % (len(got), len(wants))
        for g, (cmd, w) in zip(got, wants):
            assert g == w, "mismatch after %r\n C  %s\n py %s" % (cmd[:120], g, w)
    print("OK: %d checks, C and Python codecs agree" % checks)


if __name__ == "__main__":
    main()
