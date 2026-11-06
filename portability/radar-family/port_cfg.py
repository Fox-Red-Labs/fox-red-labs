#!/usr/bin/env python3
"""Rewrite an IWRL6432BOOST people-counting .cfg for another chip of the family, on mmWave L SDK 05.05.04.02.

  port_cfg.py <in.cfg> <device> [out.cfg]     device: iwrl6432 awrl6432 iwrl6432aop iwrl1432 awrl1432
  port_cfg.py --selftest

What changes, and where each rule comes from (SDK 05.05.04.02 sources and profiles, read 2026-11-03):
  every device  trackingCfg keeps 4 values. 05.05 reads value 5 as boresightFilteringEnable and works out
                maximum velocity, velocity resolution and frame time from the chirp itself; the 05.04 extra
                values (61.4 191.8 250) would land 61 in boresight filtering (utils/tracker_utils.c).
  *aop          antenna on package: TDM MIMO (chirpComnCfg value 5 = 1, not 4), TI's AOP antenna map and
                2.5 mm spacing, all six RX phases +1, steerVecCorr 1 1 1 (profiles/xwrL64xx-aop).
  *1432         76 to 81 GHz: chirp start frequency +17 GHz (60.5 becomes 77.5, as in TI's own profile pairs);
                antenna spacing kept at 2.418 mm as TI's 1432 profile does (lambda/2 at 62 GHz; at 78 GHz it is
                1.92 mm): angles are unverified until a 1432 board is measured.
The sweep must fit the device's band (TI product pages: 57-64 GHz FCCSP, 57-63.5 AOP, 76-81 1432)."""
import sys

BANDS = {"iwrl6432": (57, 64), "awrl6432": (57, 64), "iwrl6432aop": (57, 63.5), "iwrl1432": (76, 81), "awrl1432": (76, 81)}
AOP_ANT = "antGeometryCfg 1 1 1 0 0 1 1 3 1 2 0 3 2.5 2.5"
AOP_PHASE = "compRangeBiasAndRxChanPhase 0.0" + " 1.00000 0.00000" * 6

def port(lines, dev):
    if dev not in BANDS:
        raise SystemExit(f"device must be one of {', '.join(BANDS)}")
    out, notes, ramp, slope, start = [], [], None, None, None
    for raw in lines:
        tok = raw.split("%")[0].split()
        if not tok:
            out.append(raw); continue
        cmd = tok[0]
        if cmd == "trackingCfg" and len(tok) > 5:
            notes.append(f"trackingCfg: dropped {' '.join(tok[5:])} (05.04 form; 05.05 computes them)")
            tok = tok[:5]
        if cmd == "chirpComnCfg":
            ramp = float(tok[6])
            if dev.endswith("aop") and tok[5] != "1":
                notes.append(f"chirpComnCfg: MIMO pattern {tok[5]} -> 1 (TDM, AOP)"); tok[5] = "1"
        if cmd == "chirpTimingCfg":
            slope, start = float(tok[4]), float(tok[5])
            if "1432" in dev and start < 70:
                start += 17; notes.append(f"chirpTimingCfg: start {tok[5]} -> {start:g} GHz"); tok[5] = f"{start:g}"
        if dev.endswith("aop") and cmd == "antGeometryCfg":
            tok = AOP_ANT.split(); notes.append("antGeometryCfg: AOP map, 2.5 mm")
        if dev.endswith("aop") and cmd == "compRangeBiasAndRxChanPhase":
            tok = AOP_PHASE.split(); notes.append("compRangeBiasAndRxChanPhase: AOP phases (all +1)")
        line = " ".join(tok)
        if dev.endswith("aop") and cmd == "antGeometryCfg":
            out.append("steerVecCorr 1 1 1"); notes.append("steerVecCorr 1 1 1 added (AOP)")
        out.append(line)
    if "1432" in dev:
        notes.append("antGeometryCfg: 2.418 mm kept as in TI's 1432 profile; check the board's antenna before trusting angles")
    lo, hi = BANDS[dev]
    if start is not None and ramp is not None and slope is not None:
        top = start + ramp * slope / 1000
        if not (lo <= start and top <= hi):
            raise SystemExit(f"sweep {start:g}-{top:.2f} GHz is outside {dev}'s {lo}-{hi} GHz band")
        notes.append(f"sweep {start:g} to {top:.2f} GHz, inside {lo}-{hi} GHz")
    head = [f"% Ported by portability/radar-family/port_cfg.py for {dev}, mmWave L SDK 05.05.04.02 motion_and_presence_detection."]
    head += [f"%  {n}" for n in notes]
    return head + out, notes

def selftest():
    src = ["chirpComnCfg 16 0 0 128 4 28 0", "chirpTimingCfg 6 32 0 40 60.5",
           "antGeometryCfg 0 0 1 1 0 2 0 1 1 2 0 3 2.418 2.418", "trackingCfg 1 2 100 3 61.4 191.8 250"]
    o, _ = port(src, "iwrl1432")
    assert "chirpTimingCfg 6 32 0 40 77.5" in o and "trackingCfg 1 2 100 3" in o, o
    o, _ = port(src, "iwrl6432aop")
    assert "chirpComnCfg 16 0 0 128 1 28 0" in o and AOP_ANT in o and "steerVecCorr 1 1 1" in o, o
    o, n = port(src, "awrl6432")
    assert "chirpTimingCfg 6 32 0 40 60.5" in o and len(n) == 2, n
    try:
        port(["chirpComnCfg 16 0 0 128 4 80 0", "chirpTimingCfg 6 32 0 40 62"], "iwrl6432aop"); raise AssertionError
    except SystemExit as e:
        assert "outside" in str(e)
    print("selftest OK")

if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        selftest(); sys.exit(0)
    if len(sys.argv) < 3:
        print(__doc__.split("\n\n")[1]); sys.exit(2)
    lines, _ = port(open(sys.argv[1]).read().splitlines(), sys.argv[2])
    text = "\n".join(lines) + "\n"
    (open(sys.argv[3], "w").write(text) if len(sys.argv) > 3 else sys.stdout.write(text))
