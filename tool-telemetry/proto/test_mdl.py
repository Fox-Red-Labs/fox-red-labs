#!/usr/bin/env python3
"""Over-the-air model install: the tag's C code (tt_mdl.c) against the Python packer and the trainer's own trees.
    python3 tool-telemetry/proto/test_mdl.py
Installs the shipped and the field-trained models through the exact GATT writes a hub sends, classifies random pulls
with both, and checks every way a package can arrive broken: a flipped byte, a chunk out of order, an early commit,
a bad child index with a good CRC, too many nodes, a package signed with another key, a forged MAC, and an install of
one tree leaving the other alone. The C HMAC-SHA256 is checked against Python's hmac module on every package.
Signed commands: right tag, key and counter accepted; replays, older counters, another tag's command, another key,
a changed command byte and out-of-range commands refused."""
import os
import random
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "ai"))
import model_pkg as M  # noqa: E402
sys.path.insert(0, HERE)
import tt_cmd as C  # noqa: E402
import json  # noqa: E402

OK, COMMITTED = 0, 1
KEY = bytes.fromhex("00112233445566778899aabbccddeeff")      # mdl_host.c's test key
OTHER = bytes(16)
ERR = lambda code: 10 + code  # noqa: E731


def build():
    exe = os.path.join(tempfile.mkdtemp(), "mdl_host")
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-I", HERE, "-o", exe,
                    os.path.join(HERE, "mdl_host.c"), os.path.join(HERE, "tt_mdl.c")], check=True)
    return exe


def predict(nodes, x):
    n = nodes[0]
    while "leaf" not in n:
        n = nodes[n["l"]] if x[n["f"]] <= n["thr"] else nodes[n["r"]]
    return n["leaf"]


def main():
    exe = build()
    cmds, expect = [], []

    def ask(cmd, want):
        cmds.append(cmd)
        expect.append(want)

    def install(pkg, want_last=COMMITTED, want_finish=OK):
        ws = M.writes(pkg)
        for w in ws[:-1]:
            ask("W " + w.hex(), str(OK))
        ask("W " + ws[-1].hex(), str(want_last))
        if want_last == COMMITTED:
            ask("F %d" % pkg[0], str(want_finish))

    r = random.Random(7)
    models = {}
    for name in ("out", "out-collected"):
        p = os.path.join(HERE, "..", "ai", name, "model.json")
        if os.path.exists(p):
            models[name] = json.load(open(p))["models"]
    checks = 0
    ask("V", "0")
    ask("C 100 3000 3200 2000 3000 20000 158", "-1")               # nothing installed: the built-in model answers
    for ver, (name, m) in enumerate(sorted(models.items()), start=1):
        pk = [M.pack(m["full"], 0, ver, KEY), M.pack(m["speed"], 1, ver, KEY)]
        install(pk[0])
        install(pk[1])
        ask("V", str(ver))
        ask("S", struct.pack("<HHBBH", ver, ver, 3, 0, len(pk[1]) - 22).hex())
        for _ in range(1500):
            x = dict(dur10=r.randint(0, 3000), ravg=r.randint(0, 4000), rmax=r.randint(0, 4500), iavg=r.randint(0, 12000),
                     ipk=r.randint(0, 20000), vmin=r.randint(15000, 21000), tc=r.randint(0, 255))
            if r.random() < 0.3:
                x["iavg"] = x["ipk"] = 0
            tree = m["speed"] if x["iavg"] == 0 and x["ipk"] == 0 else m["full"]
            want = predict(tree, x)
            assert M.walk(pk[1] if tree is m["speed"] else pk[0], x) == want
            ask("C %d %d %d %d %d %d %d" % (x["dur10"], x["ravg"], x["rmax"], x["iavg"], x["ipk"], x["vmin"], x["tc"] - 128),
                str(want))
    good = M.pack(models[sorted(models)[0]]["full"], 0, 9, KEY)
    # signed with another key: every check but the MAC passes, and it never goes live
    install(M.pack(models[sorted(models)[0]]["full"], 0, 9, OTHER), COMMITTED, 6)
    ask("V", "0")
    ask("S", struct.pack("<HHBBH", 0, len(models), 2, 6, len(good) - 22).hex())
    # a forged MAC on good nodes
    forged = bytearray(good); forged[-1] ^= 1
    install(bytes(forged), COMMITTED, 6)
    ask("C 100 3000 3200 2000 3000 20000 30", "-1")
    # a flipped node byte: the CRC refuses it and the slot stays off (built-in model answers)
    bad = bytearray(good); bad[20] ^= 0x40
    install(bytes(bad), ERR(4))
    ask("C 100 3000 3200 2000 3000 20000 30", "-1")
    ask("V", "0")
    # a chunk out of order
    ws = M.writes(good)
    ask("W " + ws[0].hex(), "0"); ask("W " + ws[2].hex(), str(ERR(2)))
    # commit before the last chunk
    ask("W " + ws[0].hex(), "0"); ask("W " + ws[1].hex(), "0"); ask("W 03" + "00" * 16, str(ERR(2)))
    # data with no BEGIN, commit with no BEGIN, unknown opcode, short BEGIN
    ask("W 020000aa", str(ERR(3))); ask("W 03", str(ERR(3))); ask("W 09", str(ERR(1))); ask("W 0100", str(ERR(2)))
    # a child index pointing backwards, with a CRC that matches: the tree check refuses it
    body = bytearray(good[6:]); body[3] = 0                     # node 0's left child is node 0: a loop
    body = body[:-16]
    hdr = good[:4]
    crc = M.crc16(bytes(body), M.crc16(hdr))
    install(hdr + struct.pack("<H", crc) + bytes(body) + good[-16:], ERR(5))
    # too many nodes
    ask("W 01000100%02x0000" % 41, str(ERR(2)))
    # the speed tree survives an install of the current tree, and a good install after all that works
    install(good)
    ask("V", "9")
    # signed commands: right tag, right key and a newer counter only
    tag, other = "40:F3:B0:xx:xx:xx", "2C:D3:AD:xx:xx:xx"
    ah = C.addr_bytes(tag).hex()
    for cmd, duty in (("stop", 0), ("lock", 0), ("unlock", 0), ("run", 25)):
        ask("K %s 41 %s" % (ah, C.sign(KEY, tag, cmd, duty, 42).hex()), "0 %d %d 42" % (C.CMDS[cmd], duty))
    ask("K %s 42 %s" % (ah, C.sign(KEY, tag, "lock", 0, 42).hex()), "2 0 0 0")            # replayed
    ask("K %s 50 %s" % (ah, C.sign(KEY, tag, "lock", 0, 42).hex()), "2 0 0 0")            # older than the last
    ask("K %s 0 %s" % (ah, C.sign(KEY, other, "lock", 0, 7).hex()), "1 0 0 0")           # signed for another tag
    ask("K %s 0 %s" % (ah, C.sign(OTHER, tag, "lock", 0, 7).hex()), "1 0 0 0")           # another key
    bad = bytearray(C.sign(KEY, tag, "lock", 0, 7)); bad[0] = 4
    ask("K %s 0 %s" % (ah, bytes(bad).hex()), "1 0 0 0")                                  # lock turned into run
    ask("K %s 0 %s" % (ah, C.sign(KEY, tag, 9, 0, 7).hex()), "3 0 0 0")                  # signed but not a command
    ask("K %s 0 %s" % (ah, C.sign(KEY, tag, "run", 90, 7).hex()), "3 0 0 0")             # duty over 40
    ask("K %s 0 %s" % (ah, C.sign(KEY, tag, "lease", 200, 7).hex()), "0 5 200 7")        # a 200-minute lease
    ask("K %s 0 %s" % (ah, C.sign(KEY, tag, "lock", 3, 7).hex()), "3 0 0 0")             # lock takes no argument
    ask("K %s 0 %s" % (ah, C.sign(KEY, tag, 6, 0, 7).hex()), "3 0 0 0")                 # no command 6
    for _ in range(200):
        ctr, last = r.randint(1, 2**32 - 1), r.randint(0, 2**32 - 1)
        ask("K %s %d %s" % (ah, last, C.sign(KEY, tag, "run", 10, ctr).hex()), "0 4 10 %d" % ctr if ctr > last else "2 0 0 0")
    out = subprocess.run([exe], input="\n".join(cmds) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    for i, (c, want) in enumerate(zip(cmds, expect)):
        if out[i] != want:
            raise SystemExit("FAIL at %d: %s -> %s, want %s" % (i, c[:60], out[i], want))
        checks += 1
    print("OK: %d checks, the tag's C install and walk agree with the trainer's trees" % checks)


if __name__ == "__main__":
    main()
