#!/usr/bin/env python3
"""Fast start and stop campaign, power-tool style (predictions committed first:
am13e230/foc-drv8376/results/faststart-20261019/expectations.md). Our code; the guard in frl_guard.h bounds every run.
Nothing is flashed: every lever is a MotorVision table write made while the motor is idle, and restored after each set.

  python3 tools/frl/lock.py exec --serial AL130001 --op motor-faststart -- python3 tools/motorlink/faststart.py OUTDIR [SET ...]

Sets (three runs each, reference 40 Hz electrical):
  A  default start (flying start on: align 5 s, open loop 10 Hz/s to 30 Hz), coast stop      P1, P7
  B  open-loop ramp 30 Hz/s, coast stop                                                      P2, P7
  C  open-loop ramp 60 Hz/s, coast stop                                                      P3, P7
  D  Hall torque start (flying start off), slew 20 Hz/s, coast stop                         P4, P6, P7
  E  Hall torque start, slew 100 Hz/s, ramp stop at 100 Hz/s                                 P5, P6, P8
  F  Hall torque start, slew 100 Hz/s, ramp stop at 200 Hz/s                                 P9
  G  as D with the Hall offset (angleAdj_deg) applied                                        H3 (expectations-hall-offset.md)
  H  Hall torque start, offset applied, slew 100 Hz/s, coast stop                            H4
Stops the campaign on an over-current trip (3), a clock check (13), a stop over current (14), a bus out of range (5, 7)
or host lost (2). A controller fault (4) is recorded and the next run goes on: P3 predicts one.
"""
import json, pathlib, sys, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import campaign as C
import link as L

Q, P = "motorSetVars_M1.", "motorVars_M1.inputCtrl."
NAMES = ["speed_Hz", "Is_A", "VdcBus_V", "frlGuard.hallEdgesA", "frlGuard.hallEdgesB", "frlGuard.hallEdgesC",
         "frlGuard.state", "frlGuard.reason", "motorState"]
DEFAULTS = {Q + "accelerationStart_Hzps": 10.0, Q + "accelerationMax_Hzps": 20.0, P + "flagEnableFlyingStart": True, "frlGuard.angleAdj_deg": 0.0}
ADJ = 57.0   # the Hall offset from the 2026-10-19 sweep (minimum current near 57 degrees: results/faststart-20261019/hall-offset)
REF = 40.0
STOP_RUN = {2, 3, 5, 7, 13, 14}
SETS = {
    "A": dict(cfg={}, stop_at=11.0, stop="coast"),
    "B": dict(cfg={Q + "accelerationStart_Hzps": 30.0}, stop_at=9.0, stop="coast"),
    "C": dict(cfg={Q + "accelerationStart_Hzps": 60.0}, stop_at=9.0, stop="coast"),
    "D": dict(cfg={P + "flagEnableFlyingStart": False}, stop_at=5.0, stop="coast"),
    "E": dict(cfg={P + "flagEnableFlyingStart": False, Q + "accelerationMax_Hzps": 100.0}, stop_at=3.0, stop="ramp", ramp=100.0),
    "F": dict(cfg={P + "flagEnableFlyingStart": False, Q + "accelerationMax_Hzps": 100.0}, stop_at=3.0, stop="ramp", ramp=200.0),
    "G": dict(cfg={P + "flagEnableFlyingStart": False, "frlGuard.angleAdj_deg": ADJ}, stop_at=5.0, stop="coast"),
    "H": dict(cfg={P + "flagEnableFlyingStart": False, Q + "accelerationMax_Hzps": 100.0, "frlGuard.angleAdj_deg": ADJ}, stop_at=3.0, stop="coast"),
}
RUNS = int(__import__('os').environ.get('FS_RUNS', '3'))
TAIL = 4.0   # seconds logged after the stop: the 0.3 s the campaign used cannot see a coast-down. The heartbeat keeps going:
# the board stops streaming about 1 s after the last host packet (seen 2026-10-19), and the guard ignores a beat at idle


def configure(k, cfg):
    want = dict(DEFAULTS); want.update(cfg)
    for n, v in want.items():
        k.write(n, v)
    got = C.snapshot(k, list(want), 0.4)
    return {n: got.get(n.split(".")[-1]) for n in want}


def stopper(k, s):
    def coast(col):
        col.mark("stop_write"); C.raw_write(k, C.RUN, False); col.pump(TAIL)

    def ramp(col):
        col.mark("ramp_write")
        C.raw_write(k, Q + "accelerationMax_Hzps", s["ramp"]); C.raw_write(k, C.SPEEDREF, 0.0)
        # the speed reference reaches zero in REF / ramp seconds; the loop then holds the rotor near zero; run flag off after a margin
        col.pump(REF / s["ramp"] + 0.4)
        col.mark("stop_write"); C.raw_write(k, C.RUN, False); col.pump(TAIL)
    return coast if s["stop"] == "coast" else ramp


def main():
    out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    sets = sys.argv[2:] or list(SETS)
    log = []
    k = L.Link()
    try:
        C.PRE = {}
        for name in sets:
            s = SETS[name]
            readback = configure(k, s["cfg"])
            print(name, "settings read back", readback, flush=True)
            for i in range(1, RUNS + 1):
                tag = f"{name}{i}"
                cap_ms = int((s["stop_at"] + 3.0) * 1000)
                _, _, _, post = C.one_run(k, tag, out, REF, cap_ms, [(s["stop_at"], stopper(k, s))], NAMES, tail=0.0)
                rec = {"set": name, "run": i, "tag": tag, "settings": readback, "post": post, "pre": C.PRE}
                log.append(rec)
                (out / "runs.json").write_text(json.dumps(log, indent=1))
                if post.get("reason") in STOP_RUN:
                    raise SystemExit(f"campaign stopped: {tag} ended with guard reason {post.get('reason')}")
                time.sleep(2.0)   # standstill before the next start, so the guard's standstill-offset check sees a stopped rotor
            configure(k, {})
    finally:
        try:
            C.raw_write(k, C.RUN, False); k.read(0.2); configure(k, {})
        finally:
            k.close()


if __name__ == "__main__":
    main()
