#!/usr/bin/env python3
"""Package the built Bluetooth light: strip the ELF, make the flash .hex, write manifest.json and NOTICE.txt.
   Called by build.sh. On the CC2755P10 the .hex carries the board's own CCFG and SCFG (known-good/) through
   cc2755p10/thread/make_flash_hex.py, which refuses an ELF whose configuration differs from them."""
import datetime as dt
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

board, kit, elf_in, sdk, sysconfig, cgt, repo = sys.argv[1:8]
repo, sdk, cgt = Path(repo), Path(sdk), Path(cgt)
here = Path(__file__).resolve().parent
out = repo / "prebuilt" / board / "ble-light"
out.mkdir(parents=True, exist_ok=True)
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
UUID = "8a570001-4724-4979-ab46-406d217495e5"

hex_p, elf_p = out / "ble-light.hex", out / "ble-light.out"
if board == "cc2755p10":
    kg = here / "known-good"
    mk = subprocess.run([sys.executable, "-I", str(repo / "cc2755p10" / "thread" / "make_flash_hex.py"), elf_in,
                         str(kg / "ccfg.bin"), str(kg / "scfg.bin"), str(hex_p)], capture_output=True, text=True)
    if mk.returncode:
        print(mk.stdout + mk.stderr)
        sys.exit(1)
    cfg_note = {"ccfg": {"addr": "0x4E020000", "bytes": 2048, "source": "cc2755p10/ble-light/known-good/ccfg.bin", "sha256": sha(kg / "ccfg.bin")},
                "scfg": {"addr": "0x4E040000", "bytes": 1024, "source": "cc2755p10/ble-light/known-good/scfg.bin", "sha256": sha(kg / "scfg.bin")},
                "origin": "read from the LP-EM-CC2755P10 on probe LB07021W on 2026-10-15 (web flasher save); the build's own CCFG and SCFG "
                          "match them except the CCFG user record (0x750 to 0x7CF) and the CRC fields, which the image takes from these files"}
else:
    subprocess.run([str(cgt / "bin" / "tiarmobjcopy"), "-O", "ihex", elf_in, str(hex_p)], check=True)
    cfg_note = {"origin": "the CCFG the SDK's SysConfig built into the ELF (CRCs filled by the SDK's crc_tool); not compared with a board yet"}
shutil.copy2(elf_in, elf_p)
subprocess.run([str(cgt / "bin" / "tiarmstrip"), "--strip-all", str(elf_p)], check=True)

# address ranges actually written by the .hex
rng, ext = [], 0
for ln in hex_p.read_text().split():
    n, a, t = int(ln[1:3], 16), int(ln[3:7], 16), int(ln[7:9], 16)
    if t == 4:
        ext = int(ln[9:13], 16) << 16
    elif t == 2:
        ext = int(ln[9:13], 16) << 4
    elif t == 0:
        s = ext + a
        if rng and rng[-1][1] == s:
            rng[-1][1] = s + n
        else:
            rng.append([s, s + n])


def name(a):
    return "ccfg" if a == 0x4E020000 else "scfg" if a == 0x4E040000 else "main flash"


segs = [{"region": name(a), "start": f"0x{a:08X}", "end": f"0x{b:08X}", "bytes": b - a} for a, b in rng]
main_end = max(b for a, b in rng if a < 0x4E000000)
sdk_ver = re.search(r"f3_sdk_(\d+)_(\d+)_(\d+)_(\d+)", sdk.name)
chip = {"cc2755p10": {"deviceid": "0x6BB9802F", "partid": "0x80E66301"}}.get(board, {"deviceid": "unknown: not read for this build"})
man = {
    "format": "frl-prebuilt-1", "board": board, "kit": kit.replace("_", "-"),
    "revisions_tested": ["unknown: not run on a board yet"],
    "chip": chip, "recipe": "ble-light",
    "ble": {"device_name": "FRL Light", "advertising": "connectable; flags and the 128-bit service UUID in the advertising data, name in the scan response",
            "service_uuid": UUID,
            "characteristics": {
                "colour": {"uuid": "8a570002-4724-4979-ab46-406d217495e5", "properties": "read, write", "format": "3 bytes R,G,B; a channel is on at 128 or more"},
                "pattern": {"uuid": "8a570003-4724-4979-ab46-406d217495e5", "properties": "write", "format": "up to 16 steps of R,G,B (3 bytes) + duration ms (uint16 little endian); empty = solid colour"},
                "status": {"uuid": "8a570004-4724-4979-ab46-406d217495e5", "properties": "read, notify", "format": "ASCII, e.g. 'up=12s step=2/4' or 'up=12s solid', notified once a second"}},
            "security": "open: no pairing or bonding is requested or required",
            "leds": "red and green user LEDs of the kit (blue has no LED and is ignored)"},
    "image": {"file": hex_p.name, "format": "Intel HEX", "sha256": sha(hex_p), "bytes": hex_p.stat().st_size,
              "data_bytes": sum(s["bytes"] for s in segs), "main_flash_end": f"0x{main_end:X}", "segments": segs},
    "elf": {"file": elf_p.name, "sha256": sha(elf_p), "bytes": elf_p.stat().st_size, "note": "stripped; the .hex was made from the unstripped ELF"},
    "config": cfg_note,
    "source": {"repo_commit": subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip(),
               "example": f"examples/rtos/{kit}/ble/basic_ble", "added": ["frl_light.c", "frl_light.h"],
               "sha256_frl_light_c": sha(here / "frl_light.c"), "rebuild": f"cc2755p10/ble-light/build.sh {board}"},
    "tools": {"sdk": {"version": ".".join(sdk_ver.groups()) if sdk_ver else "unknown"},
              "sysconfig": {"version": re.search(r"sysconfig_([\d.]+)", sysconfig).group(1)},
              "compiler": {"version": re.search(r"armllvm_([\d.]+\w*)", cgt.name).group(1)}},
    "expect": "advertises as FRL Light; a Web Bluetooth page can connect without a PIN, set the LED colour, play a pattern and read the status",
    "made": dt.datetime.now().isoformat(timespec="seconds"), "release_cleared": False,
    "release_note": "Built from TI's basic_ble example (BSD-3-Clause, NOTICE.txt) plus our frl_light.c. Not cleared for public distribution; not yet run on a board.",
}
(out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
lic = (here / "frl_light.c").read_text().split("******************************************************************************/", 1)[0]
(out / "NOTICE.txt").write_text(f"Texas Instruments' basic_ble example for the {kit.replace('_', '-')} from the SimpleLink Low Power F3 SDK {man['tools']['sdk']['version']}, "
                                "with frl_light.c (derived from TI's simple_gatt_profile.c) added. TI's notice from that source:\n\n" + lic.strip() + "\n")
print(f"OK: {out.relative_to(repo)}: hex sha256 {man['image']['sha256']}")
for s in segs:
    print(f"  {s['region']:10} {s['start']} to {s['end']} ({s['bytes']} bytes)")
