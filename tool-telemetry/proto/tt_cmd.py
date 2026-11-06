#!/usr/bin/env python3
"""Signed commands to a tool, through its tag (stage two; format in proto/tt_mdl.h and PROTOCOL.md section 7).
Standard library only. Used by homebase/sendcmd.py and proto/test_mdl.py."""
import hashlib
import hmac
import struct

CMDS = {"stop": 1, "lock": 2, "unlock": 3, "run": 4, "lease": 5}
NAMES = {v: k for k, v in CMDS.items()}


def addr_bytes(addr):
    """'40:F3:B0:xx:xx:xx' as shown, to the six bytes the tag signs over (least significant first, as advertised)."""
    b = bytes.fromhex(addr.replace(":", ""))
    if len(b) != 6:
        raise ValueError("a Bluetooth address has 6 bytes")
    return b[::-1]


def sign(key, addr, cmd, duty, counter):
    """The 18 bytes the hub writes to the tag's 0xFAB3: cmd, duty, counter, then 12 bytes of HMAC-SHA256."""
    c = CMDS[cmd] if isinstance(cmd, str) else cmd
    body = struct.pack("<BBI", c, duty, counter)
    return body + hmac.new(key, b"\xc0" + addr_bytes(addr) + body, hashlib.sha256).digest()[:12]
