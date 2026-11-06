#!/usr/bin/env python3
"""Package one built radio demo: strip the ELF, make the flash .hex, find the parameter and status blocks, write manifest.json and NOTICE.txt.
   Called by build.sh. On the CC2755P10 the .hex carries the board's own CCFG and SCFG (cc2755p10/ble-light/known-good/) through
   cc2755p10/thread/make_flash_hex.py; on the CC2340R5 it is the hex the SDK build made (CCFG from SysConfig, not compared with a board)."""
import datetime as dt, hashlib, json, re, shutil, subprocess, sys
from pathlib import Path

board, kit, demo, elf_in, sdk, sysconfig, cgt, repo = sys.argv[1:9]
repo, sdk, cgt = Path(repo), Path(sdk), Path(cgt)
here = Path(__file__).resolve().parent
name = f"radio-{demo}"
out = repo / "prebuilt" / board / name
out.mkdir(parents=True, exist_ok=True)
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
hex_p, elf_p = out / f"{name}.hex", out / f"{name}.out"

# symbols from the unstripped ELF
nm = subprocess.run([str(cgt / "bin" / "tiarmnm"), elf_in], capture_output=True, text=True, check=True).stdout
sym = lambda n: int(re.search(rf"^([0-9a-fA-F]+)\s+\w\s+{n}$", nm, re.M).group(1), 16)
try:
    status_addr = sym("frl_status")
except AttributeError:      # TI's Zigbee example has neither block
    status_addr = None

if board == "cc2755p10":
    kg = repo / "cc2755p10" / "ble-light" / "known-good"
    mk = subprocess.run([sys.executable, "-I", str(repo / "cc2755p10" / "thread" / "make_flash_hex.py"), elf_in, str(kg / "ccfg.bin"), str(kg / "scfg.bin"), str(hex_p)],
                        capture_output=True, text=True)
    if mk.returncode:
        print(mk.stdout + mk.stderr); sys.exit(1)
    cfg_note = {"ccfg": {"addr": "0x4E020000", "bytes": 2048, "source": "cc2755p10/ble-light/known-good/ccfg.bin", "sha256": sha(kg / "ccfg.bin")},
                "scfg": {"addr": "0x4E040000", "bytes": 1024, "source": "cc2755p10/ble-light/known-good/scfg.bin", "sha256": sha(kg / "scfg.bin")},
                "origin": "read from the LP-EM-CC2755P10 on probe LB07021W on 2026-10-15 (web flasher save)"}
else:
    subprocess.run([str(cgt / "bin" / "tiarmobjcopy"), "-O", "ihex", elf_in, str(hex_p)], check=True)
    cfg_note = {"origin": "the CCFG the SDK's SysConfig built into the ELF (CRCs filled by the SDK's crc_tool); not compared with a board yet"}
shutil.copy2(elf_in, elf_p)
subprocess.run([str(cgt / "bin" / "tiarmstrip"), "--strip-all", str(elf_p)], check=True)

rng, ext = [], 0
for ln in hex_p.read_text().split():
    n, a, t = int(ln[1:3], 16), int(ln[3:7], 16), int(ln[7:9], 16)
    if t == 4: ext = int(ln[9:13], 16) << 16
    elif t == 2: ext = int(ln[9:13], 16) << 4
    elif t == 0:
        s = ext + a
        if rng and rng[-1][1] == s: rng[-1][1] = s + n
        else: rng.append([s, s + n])
region = lambda a: "ccfg" if a == 0x4E020000 else "scfg" if a == 0x4E040000 else "main flash"
segs = [{"region": region(a), "start": f"0x{a:08X}", "end": f"0x{b:08X}", "bytes": b - a} for a, b in rng]
main_end = max(b for a, b in rng if a < 0x4E000000)
sdk_ver = re.search(r"f3_sdk_(\d+)_(\d+)_(\d+)_(\d+)", sdk.name)
sys.path.insert(0, str(repo / "tools" / "webflash"))
from params import p as _p, MAGIC as _MAGIC, FORMAT as _FORMAT      # the shared parameter block's own helpers
TABLE = {"cc2755p10": 0x000DF800, "cc2340r5": 0x0007F800}[board]
ITEMS = {"blink": [_p("on_ms", "LED on", "ms", 10, 60000, 500, 10), _p("off_ms", "LED off", "ms", 10, 60000, 500, 10)],
         "message": [_p("period_ms", "Time between lines", "ms", 50, 60000, 1000, 50, "the line counts up each time")],
         "button": [_p("repeat_ms", "Repeat the buttons line every", "ms", 100, 60000, 1000, 100, "lines also print at once on every press or release")]}
PARAMS = ({"kind": "params", "format": _FORMAT, "addr": f"0x{TABLE:08X}", "magic": f"0x{_MAGIC:08X}", "image_id": {"blink": 6, "message": 7, "button": 8}[demo],
           "items": ITEMS[demo], "region": "the last 2 kB sector below the security firmware; one sector write through the chip's mailbox, then a reset"}
          if demo in ITEMS else None)
DEMO = {
    "blink": {"expect": "the red LED (LED0) is on for on_ms and off for off_ms, repeating; frl_status.counter rises at every toggle",
              "params": {}},
    "message": {"expect": "prints \"> Hello from my LaunchPad N\" on the backchannel UART at 115200 baud every period_ms, N counting; LED0 toggles with each line; frl_status.counter = N",
                "params": {}},
    "zigbee": {"expect": "starts TI's Zigbee on/off light (a relaying node): it logs its stack messages on the backchannel UART at 115200 baud and waits for a network; joining a coordinator was not tested",
               "params": {}},
    "button": {"expect": "LED0 follows BTN-1 and LED1 follows BTN-2 (pressed = lit); prints \"buttons: BTN-1 up, BTN-2 down, presses N\" on every change and every period_ms; frl_status.aux holds the buttons (bit 0 BTN-1, bit 1 BTN-2), counter the presses",
               "params": {}},
}[demo]
man = {
    "format": "frl-prebuilt-1", "board": board, "kit": kit.replace("_", "-"),
    "revisions_tested": ["unknown: not run on a board yet" if board != "cc2755p10" else "LP-EM-CC2755P10, probe LS5600RE, 2026-10-22 (see data/results/cc2755p10.csv)"],
    "chip": {"deviceid": {"cc2755p10": "0x6BB9802F", "cc2340r5": "0x6BB8402F"}[board]}, "recipe": f"radio-{demo}",
    "console": None if demo == "blink" else {"uart": "the XDS110 backchannel (the probe's second serial port, /dev/cu.usbmodem<serial>1 on a Mac)", "baud": 115200, "format": "8N1"},
    "params": PARAMS,
    "status": None if status_addr is None else {"addr": f"0x{status_addr:08X}", "magic": "0x53524C46", "words": ["magic", "demo", "counter", "aux"], "demo_id": {"blink": 1, "message": 2, "button": 3}[demo]},
    "image": {"file": hex_p.name, "format": "Intel HEX", "sha256": sha(hex_p), "bytes": hex_p.stat().st_size, "data_bytes": sum(s["bytes"] for s in segs),
              "main_flash_end": f"0x{main_end:X}", "segments": segs},
    "elf": {"file": elf_p.name, "sha256": sha(elf_p), "bytes": elf_p.stat().st_size, "note": "stripped; the .hex was made from the unstripped ELF"},
    "config": cfg_note,
    "source": {"repo_commit": subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip(),
               "example": f"examples/nortos/{kit}/drivers/empty" if demo != "zigbee" else f"examples/rtos/{kit}/zigbee/onoff_light", "added": ["frl_demo.c", "frl_demo.syscfg"] if demo != "zigbee" else [], "sha256_frl_demo_c": sha(here / "frl_demo.c"),
               "rebuild": f"cc2755p10/radio-demos/build.sh {board} {demo}"},
    "tools": {"sdk": {"version": ".".join(sdk_ver.groups()) if sdk_ver else "unknown"}, "sysconfig": {"version": re.search(r"sysconfig_([\d.]+)", sysconfig).group(1)},
              "compiler": {"version": re.search(r"armllvm_([\d.]+\w*)", cgt.name).group(1)}},
    "expect": DEMO["expect"], "made": dt.datetime.now().isoformat(timespec="seconds"), "release_cleared": False,
    "release_note": "Built from TI's drivers/empty example (BSD-3-Clause, NOTICE.txt) plus our frl_demo.c. Not cleared for public distribution.",
}
(out / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
lic = ((sdk / "examples" / "rtos" / kit / "zigbee" / "onoff_light" / "on_off_light.c") if demo == "zigbee" else (sdk / "examples" / "nortos" / kit / "drivers" / "empty" / "empty.c")).read_text().split("*/", 1)[0]
(out / "NOTICE.txt").write_text(f"Texas Instruments' {'zigbee/onoff_light' if demo == 'zigbee' else 'drivers/empty'} example for the {kit.replace('_', '-')} from the SimpleLink Low Power F3 SDK {man['tools']['sdk']['version']}, "
                                + ("unchanged" if demo == "zigbee" else "with frl_demo.c added") + ". TI's notice from that source:\n\n" + lic.strip() + "\n */\n")
print(f"OK: {out.relative_to(repo)}: hex sha256 {man['image']['sha256']} params {man['params'] and man['params']['addr']} status {man['status'] and man['status']['addr']}")
for s in segs: print(f"  {s['region']:10} {s['start']} to {s['end']} ({s['bytes']} bytes)")
