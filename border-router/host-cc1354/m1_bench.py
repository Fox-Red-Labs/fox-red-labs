#!/usr/bin/env python3
"""Milestone M1 on the bench: the CC1354P10 host drives the CC2340R5 RCP over the three wires and forms a Thread network.

  python3 m1_bench.py run <host.out> <rcp.hex>   back up both boards, flash the RCP, load the host, test from the console
  python3 m1_bench.py restore <run dir>          put both boards back from that run's backups (RCP: DSLite, host: CCS)

Order of work, same safety rules as tools/demo_flash.py:
  1. CC2340R5: read flash and CCFG with DSLite (length in words), refuse if blank, write restore.hex, flash the RCP
     image (CCFG CRCs already filled), toggle reset with xds110reset so it boots (a DSLite flash alone does not).
  2. CC1354P10: one CCS scripting session backs up flash and CCFG, loads the host image and runs it (a DSLite
     flash would not start it without a power cycle).
  3. Console on the host's XDS110 UART at 921600 (tools/ot_cli.py): version, eui64, state; form a network; state,
     panid, channel, rloc16, ipaddr. The eui64 must be the CC2340R5's, not the CC1354's: the radio is the other chip.
Backups and the log go to out/border-router-m1/<time>/ (outside the repo). A summary is printed.
"""
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from demo_flash import DSLITE_SH, ihex, read_mem, sh  # noqa: E402

BOARDS = json.load(open(ROOT / "tools/board_sweep/boards.json"))
RCP, HOST = BOARDS["LP_EM_CC2340R5"], BOARDS["LP_EM_CC1354P10_1"]
RCP_CCXML = (ROOT / RCP["ccxml"]).resolve()
HOST_CCXML = (ROOT / HOST["ccxml"]).resolve()
HOST_PORT = f"/dev/cu.usbmodem{HOST['probe']}1"
XDS110RESET = "/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xds110reset"
CCS_RUN = "/Applications/ti/ccs2101/ccs/scripting/python/run.sh"
OT_CLI = ROOT / "tools/ot_cli.py"


def log(d, msg):
    line = f"{time.time():.3f} {msg}"
    print(line, flush=True)
    with open(d / "m1.log", "a") as f:
        f.write(line + "\n")


def console(d, *cmds):
    rc, out = sh([sys.executable, str(OT_CLI), HOST_PORT, *cmds], timeout=120)
    log(d, f"console rc={rc}\n{out.strip()}")
    return out


def rcp_backup(d):
    bdir = d / "rcp"
    bdir.mkdir(parents=True, exist_ok=True)
    chunks = [(start, read_mem(DSLITE_SH, RCP_CCXML, start, length, bdir / f"backup_{start:#x}.bin"))
              for start, length in RCP["backup"]]
    if chunks[0][1][:8] in (b"\xff" * 8, b"\x00" * 8):
        sys.exit("REFUSED: CC2340R5 backup looks blank. Nothing was flashed.")
    (bdir / "restore.hex").write_text(ihex(chunks))
    log(d, f"rcp backup ok {bdir}")
    return bdir


def rcp_flash(d, hexpath):
    t0 = time.time()
    rc, out = sh([DSLITE_SH, "--mode", "flash", "-c", str(RCP_CCXML), "-e", "-f", "-u", str(hexpath)], timeout=300)
    log(d, f"rcp flash rc={rc} in {time.time() - t0:.1f} s: {out.strip()[-300:]}")
    if rc:
        sys.exit("RCP flash failed")
    rc, out = sh([XDS110RESET, "-s", RCP["probe"]])
    log(d, f"rcp reset rc={rc} {out.strip()[-120:]}")


def rcp_verify_restore(d, bdir):
    chk = d / "rcp-check"
    chk.mkdir(exist_ok=True)
    same = True
    for start, length in RCP["backup"]:
        now = read_mem(DSLITE_SH, RCP_CCXML, start, length, chk / f"now_{start:#x}.bin")
        was = (bdir / f"backup_{start:#x}.bin").read_bytes()
        pages = sum(1 for i in range(0, length, 2048) if now[i:i + 2048] != was[i:i + 2048])
        log(d, f"rcp restore check {start:#x}: {pages} of {length // 2048} pages differ")
        same = same and pages == 0
    return same


def host_ccs(d, mode, image=None):
    hdir = d / "host"
    args = [CCS_RUN, str(HERE / "tools/ccs_backup_load.py"), str(HOST_CCXML), str(hdir), mode] + ([str(image)] if image else [])
    t0 = time.time()
    rc, out = sh(args, timeout=600)
    log(d, f"host ccs {mode} rc={rc} in {time.time() - t0:.1f} s: {out.strip()[-400:]}")
    if rc or ("backup ok" not in out and mode != "restore"):
        sys.exit(f"host {mode} failed")
    return hdir


def run(host_out, rcp_hex):
    d = Path.cwd() / "out/border-router-m1" / time.strftime("%Y%m%d-%H%M%S")
    d.mkdir(parents=True)
    log(d, f"M1 run: host={host_out} rcp={rcp_hex}")
    rcp_backup(d)
    rcp_flash(d, rcp_hex)
    time.sleep(1.0)
    t_load = time.time()
    host_ccs(d, "load", host_out)
    time.sleep(3.0)
    out = console(d, "version", "eui64", "extaddr", "state", "rcp version")
    if "OPENTHREAD" not in out:
        log(d, "host console did not answer: the spinel link or the host image did not come up")
        print(f"\nrun dir {d}")
        sys.exit(1)
    t_form = time.time()
    console(d, "dataset init new", "dataset commit active", "ifconfig up", "thread start")
    state = ""
    for _ in range(30):
        time.sleep(1.0)
        state = console(d, "state").strip().split("\n")[1].strip()
        if state in ("leader", "router", "child"):
            break
    log(d, f"state={state} {time.time() - t_form:.1f} s after thread start ({time.time() - t_load:.1f} s after host load)")
    console(d, "panid", "channel", "rloc16", "ipaddr", "netdata show", "counters mac")
    print(f"\nrun dir {d}\nrestore both boards with: python3 {HERE / 'm1_bench.py'} restore {d}")


def restore(run_dir):
    d = Path(run_dir)
    log(d, "restore start")
    rc, out = sh([DSLITE_SH, "--mode", "flash", "-c", str(RCP_CCXML), "-e", "-f", "-u", str((d / "rcp/restore.hex").resolve())], timeout=300)
    log(d, f"rcp restore flash rc={rc}: {out.strip()[-200:]}")
    ok = rcp_verify_restore(d, d / "rcp")
    sh([XDS110RESET, "-s", RCP["probe"]])
    log(d, f"rcp restored and reset: {'all pages identical' if ok else 'PAGES DIFFER'}")
    host_ccs(d, "restore", (d / "host/restore.hex").resolve())


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "run":
        run(Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve())
    elif len(sys.argv) == 3 and sys.argv[1] == "restore":
        restore(sys.argv[2])
    else:
        print(__doc__)
        sys.exit(2)
