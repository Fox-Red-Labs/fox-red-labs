#!/usr/bin/env python3
"""Package a trained model for installing over the air (proto/tt_mdl.h). Standard library only.
    python3 tool-telemetry/ai/model_pkg.py --model ai/out-collected/model.json --version 2 --out <dir>
Writes <dir>/model-v<N>.json: the version, where it came from, its test scores, and two packages as hex (the tree
that uses current, and the speed-only tree), each exactly the bytes a hub writes to a tag. The home base serves the
newest one at /api/model; the hub takes it at POST /api/model and installs it on every tag it hears."""
import argparse
import hashlib
import hmac
import json
import os
import struct
import time

FULL = ("dur10", "ravg", "rmax", "iavg", "ipk", "vmin", "tc")
MAX_NODES, CHUNK = 40, 17
KEY_FILE = os.path.expanduser("~/.config/frl/tool-telemetry-model.key")   # outside the repo; tag/build.sh makes it


def load_key(path=KEY_FILE):
    """The fleet's 16-byte signing key, as 32 hex digits in a file outside the repository."""
    k = bytes.fromhex(open(path).read().strip())
    if len(k) != 16:
        raise SystemExit("%s must hold 32 hex digits" % path)
    return k


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def nodes_bytes(nodes):
    out = b""
    for n in nodes:
        if "leaf" in n:
            out += struct.pack("<bHBB", -1, n["leaf"], 0, 0)
        else:
            out += struct.pack("<bHBB", FULL.index(n["f"]), n["thr"], n["l"], n["r"])
    return out


def pack(nodes, kind, ver, key):
    """BEGIN header (kind, version, count, CRC), the node bytes, then the 16-byte MAC the tag checks before using it."""
    if not 0 < len(nodes) <= MAX_NODES:
        raise SystemExit("a tree has %d nodes; a tag takes 1 to %d" % (len(nodes), MAX_NODES))
    body = nodes_bytes(nodes)
    head = struct.pack("<BHB", kind, ver, len(nodes))
    crc = crc16(body, crc16(head))
    mac = hmac.new(key, head + body, hashlib.sha256).digest()[:16]
    return struct.pack("<BHBH", kind, ver, len(nodes), crc) + body + mac


def writes(pkg):
    """The GATT writes that install one package: BEGIN, DATA in order, COMMIT with the MAC."""
    head, body, mac = pkg[:6], pkg[6:-16], pkg[-16:]
    w = [b"\x01" + head]
    for off in range(0, len(body), CHUNK):
        w.append(struct.pack("<BH", 2, off) + body[off:off + CHUNK])
    return w + [b"\x03" + mac]


def walk(pkg, x):
    """The class a tag would give, from the package bytes alone (the test checks this against the trainer)."""
    n = pkg[3]
    nodes = [struct.unpack_from("<bHBB", pkg, 6 + 5 * i) for i in range(n)]
    i = 0
    for _ in range(MAX_NODES):
        f, thr, l, r = nodes[i]
        if f < 0:
            return thr
        i = l if x[FULL[f]] <= thr else r
    return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "out-collected", "model.json"))
    ap.add_argument("--version", type=int, required=True, help="1 to 65535; 0 means the model built into the firmware")
    ap.add_argument("--out", required=True)
    ap.add_argument("--key", default=KEY_FILE, help="the fleet signing key (32 hex digits); tag/build.sh makes it")
    a = ap.parse_args()
    key = load_key(a.key)
    if not 0 < a.version < 65536:
        raise SystemExit("--version must be 1 to 65535")
    m = json.load(open(a.model))
    pk = [pack(m["models"]["full"], 0, a.version, key), pack(m["models"]["speed"], 1, a.version, key)]
    rec = dict(version=a.version, made=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), source=m.get("source", ""),
               trained_on=dict(train=m.get("train_pulls"), test=m.get("test_pulls"), split=m.get("split")),
               test_accuracy={k: m["report"][k]["test_accuracy"] for k in ("full", "speed")},
               nodes={k: len(m["models"][k]) for k in ("full", "speed")},
               packages=[p.hex() for p in pk], sha256=hashlib.sha256(b"".join(pk)).hexdigest())
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "model-v%d.json" % a.version)
    json.dump(rec, open(path, "w"), indent=1)
    print("model v%d: %d + %d nodes, %d + %d bytes, %d writes -> %s" % (
        a.version, rec["nodes"]["full"], rec["nodes"]["speed"], len(pk[0]), len(pk[1]), len(writes(pk[0])) + len(writes(pk[1])), path))


if __name__ == "__main__":
    main()
