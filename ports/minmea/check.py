#!/usr/bin/env python3
"""Read the eight result globals of the minmea test app from a running LP-MSPM0G3519 and compare with the sentences' values.

  check.py <map file> <XDS110 serial>        e.g. check.py out/mspm0g3519/minmea.map MG000001

Halts the core, reads the globals with OpenOCD, resumes. Takes no probe lock; hold one around the call if the probe is shared.
The expected values come from the two NMEA sentences in app/main.c (run-g3519.txt); lat is 481172 because main.c truncates a float.
"""
import os, re, subprocess, sys

EXPECT = {"g_rmc_ok": 1, "g_gga_ok": 1, "g_ts_ok": 1, "g_sats": 8, "g_alt_dm": 5454, "g_lat_e4": 481172, "g_lon_e4": 115166, "g_ts": 764426119}
OCD = os.path.expanduser(os.environ.get("BENCH_XPACK", "~/zephyr-toolchain/xpack-openocd-0.12.0-7"))


def main(mp, serial):
    sym = {n: int(a, 16) for a, n in re.findall(r"^\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+\(\.common:(g_\w+)\)", open(mp).read(), re.M)}
    missing = [n for n in EXPECT if n not in sym]
    if missing:
        sys.exit(f"MINMEA CHECK: FAIL (no symbol {missing} in {mp})")
    cmd = [f"{OCD}/bin/openocd", "-s", f"{OCD}/openocd/scripts", "-c", "gdb_port disabled", "-c", "tcl_port disabled", "-c", "telnet_port disabled",
           "-f", "interface/xds110.cfg", "-c", f"adapter serial {serial}", "-c", "adapter speed 1000", "-f", "target/ti_mspm0.cfg", "-c", "init", "-c", "halt"]
    for n in EXPECT:
        cmd += ["-c", f"mdw 0x{sym[n]:x}"]
    cmd += ["-c", "resume", "-c", "shutdown"]
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    got = {int(a, 16): int(v, 16) for a, v in re.findall(r"^0x([0-9a-f]+): ([0-9a-f]{8})", out.stdout + out.stderr, re.M)}
    ok = True
    for n, want in EXPECT.items():
        v = got.get(sym[n])
        v = v - (1 << 32) if v is not None and v >= 1 << 31 else v
        same = v == want
        ok &= same
        print(f"{'OK  ' if same else 'DIFF'} {n} = {v} (expected {want})")
    print("MINMEA CHECK:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]) if len(sys.argv) == 3 else __doc__)
