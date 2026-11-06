#!/usr/bin/env python3
"""Compare the output of unit_sym with OpenSSL's (via the cryptography package) for the same inputs."""
import hashlib, hmac, subprocess, sys, zlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
buf = bytes((i * 7 + 3) & 255 for i in range(200)); key = bytes(0xa0 + i for i in range(16)); nonce = bytes(range(12)); aad = b"pqc-join-v1"
exp = {"sha_0": hashlib.sha256(b"").hexdigest(), "sha_55": hashlib.sha256(buf[:55]).hexdigest(), "sha_64": hashlib.sha256(buf[:64]).hexdigest(),
       "sha_200": hashlib.sha256(buf).hexdigest(), "hmac": hmac.new(key, buf[:100], "sha256").hexdigest(),
       "hkdf": HKDF(hashes.SHA256(), 16, None, b"info").derive(buf[:32]).hex(), "crc": "%08x" % zlib.crc32(buf)}
got = {}
for line in subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True).stdout.splitlines():
    p = line.split()
    if p[0] == "gcm":
        n = int(p[1]); c = AESGCM(key).encrypt(nonce, buf[:n], aad)
        assert (p[2] if len(p) == 4 else "") == c[:-16].hex() and p[-1] == c[-16:].hex(), ("gcm", n)
    else:
        got[p[0]] = p[1]
bad = [k for k in exp if got.get(k) != exp[k]]
print("symmetric primitives match OpenSSL" if not bad else "MISMATCH: %s" % bad)
sys.exit(1 if bad else 0)
