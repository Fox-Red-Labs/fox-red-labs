#!/usr/bin/env python3
"""Capture EnergyTrace current from a TI XDS110-ET debug probe on macOS or Linux, with no IDE.

  python3 tools/energytrace/etcap.py --list
  python3 tools/energytrace/etcap.py --serial ML130001 --seconds 10 --csv out.csv [--json out.json] [--mv 3300] [--hz 2000]

How it works. CCStudio ships the XDS110 driver as a plain shared library
(ccs_base/common/uscif/libjscxds110.dylib on macOS, .so on Linux) with C entry points for
EnergyTrace: XDS_Open, ET_HardwareInfo, ET_DCDC_SetVcc, ET_Setup_Range, ET_Setup_Dig, ET_Setup,
ET_Calibrate, ET_Start, XDS_ReadDataPort, ET_Stop, ET_Cleanup, XDS_Close. This script calls them
through ctypes, decodes the probe's data-port stream, and converts tick counts to current with the
same two-point calibration CCStudio's own library uses for this probe type (open circuit and the
probe's internal 2200 ohm load, worked out from libenergytraceccs on 2026-10-07).

Stream format seen on XDS110 firmware 3.0.0.43, probe hardware id 0x28 (MSPM0 LaunchPad kits):
  byte>>5 == 1: 3 bytes, header, bytes 1..2 = sampling rate in Hz (little endian)
  byte>>5 == 2: 1 byte, epoch marker every 256 samples
  byte>>5 == 3: 4 bytes: 0x70, sequence, tick count for this sample, flags

Known limits: the probe supplies the target at the voltage you ask for (3.3 V by default; the
LaunchPad target must be powered through the probe for the reading to mean anything). Only the
ET-MSP style probe stream (hardware id 0x28) is decoded; HDR probes are detected but not decoded yet.
A chip that a debugger has touched stays out of STANDBY and STOP until a power-on reset; the probe's
XDS_PowerIsolate command does nothing on an embedded LaunchPad probe, so --power-cycle cannot force one.
Open question (2026-10-07): a sleeping LP-MSPM0L1306 reads 1.506 mA, equal to the probe's 2,200 ohm
calibration load; until the J101 jumper test is done, treat 1.5 mA as a possible floor in every reading.
"""
import argparse, ctypes as C, json, os, platform, statistics, sys, time

CCS = os.environ.get("FRL_CCS_ROOT") or os.environ.get("CCS_ROOT", "/Applications/ti/ccs2101/ccs")   # FRL_CCS_ROOT: a reader's CCStudio folder
USCIF = os.path.join(CCS, "ccs_base", "common", "uscif")
LIB = os.path.join(USCIF, "libjscxds110.dylib" if platform.system() == "Darwin" else "libjscxds110.so")

ERRORS = {-640: "ET mode not supported on this probe or device", -641: "invalid sampling frequency",
          -642: "ET slave timed out", -643: "ET slave not reachable", -644: "ET internal error"}


class Probe:
    def __init__(self, serial, et=True):
        os.chdir(USCIF)  # the driver loads libusb from ../bin relative to its own folder
        self.L = L = C.CDLL(LIB)
        def fn(name, args, res=C.c_int):
            f = getattr(L, name); f.argtypes = args; f.restype = res; return f
        self.XDS_Open = fn("XDS_Open", [C.POINTER(C.c_void_p), C.c_int, C.c_char_p, C.c_bool])
        self.XDS_Close = fn("XDS_Close", [C.POINTER(C.c_void_p)])
        self.XDS_Version = fn("XDS_Version", [C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)])
        self.ET_HardwareInfo = fn("ET_HardwareInfo", [C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.c_uint8)])
        self.ET_DCDC_GetMCUVersion = fn("ET_DCDC_GetMCUVersion", [C.c_void_p, C.POINTER(C.c_uint32)])
        self.ET_DCDC_SetVcc = fn("ET_DCDC_SetVcc", [C.c_void_p, C.c_uint16])
        self.ET_DCDC_PowerDownMCU = fn("ET_DCDC_PowerDownMCU", [C.c_void_p]); self.ET_DCDC_RestartMCU = fn("ET_DCDC_RestartMCU", [C.c_void_p])
        self.XDS_JTAGIsolate = fn("XDS_JTAGIsolate", [C.c_void_p, C.c_uint8])
        self.ET_Setup_Range = fn("ET_Setup_Range", [C.c_void_p, C.c_uint8])
        self.ET_Setup_Dig = fn("ET_Setup_Dig", [C.c_void_p, C.c_uint8])
        self.ET_Setup = fn("ET_Setup", [C.c_void_p, C.c_uint8, C.c_uint32, C.c_uint8, C.c_uint8])
        self.ET_Calibrate = fn("ET_Calibrate", [C.c_void_p, C.c_uint16, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)])
        self.ET_Start = fn("ET_Start", [C.c_void_p]); self.ET_Stop = fn("ET_Stop", [C.c_void_p]); self.ET_Cleanup = fn("ET_Cleanup", [C.c_void_p])
        self.XDS_ReadDataPort = fn("XDS_ReadDataPort", [C.c_void_p, C.c_char_p, C.c_uint32, C.POINTER(C.c_int)])
        self.h = C.c_void_p()
        rc = self.XDS_Open(C.byref(self.h), 0, serial.encode(), et)
        if rc != 0:
            raise RuntimeError(f"XDS_Open({serial}) failed: {rc}")
        self.serial = serial

    def close(self):
        if self.h.value:
            self.XDS_Close(C.byref(self.h))

    def info(self):
        fw = C.c_uint32(); hw = C.c_uint32(); self.XDS_Version(self.h, C.byref(fw), C.byref(hw))
        et = C.c_uint32(); flags = C.c_uint8(); rc = self.ET_HardwareInfo(self.h, C.byref(et), C.byref(flags))
        mcu = C.c_uint32(); self.ET_DCDC_GetMCUVersion(self.h, C.byref(mcu))
        return {"serial": self.serial, "fw": f"{fw.value >> 24}.{(fw.value >> 16) & 0xff}.{(fw.value >> 8) & 0xff}.{fw.value & 0xff}",
                "hw_id": hw.value, "et_rc": rc, "et_type": et.value, "et_flags": flags.value, "dcdc_mcu": mcu.value}


def rc_text(rc):
    return "ok" if rc == 0 else ERRORS.get(rc, f"error {rc}")


def list_probes():
    import subprocess, re
    out = subprocess.run(["ioreg", "-p", "IOUSB", "-l", "-w0"], capture_output=True, text=True).stdout
    serials = sorted(set(re.findall(r'"USB Serial Number" = "([A-Z0-9]{8})"', out)))
    for s in serials:
        try:
            p = Probe(s, et=True); i = p.info(); p.close()
            print(f"{s}: firmware {i['fw']}, hw id {i['hw_id']:#x}, EnergyTrace {'yes' if i['et_rc'] == 0 else 'no'} (type {i['et_type']}, flags {i['et_flags']})")
        except Exception as e:
            print(f"{s}: not an XDS110 I can open ({e})")


def decode(data):
    """Returns (samples as tick counts, header hz or None)."""
    i, ticks, hdr_hz = 0, [], None
    while i < len(data):
        t = data[i] >> 5
        if t == 3 and i + 3 < len(data):
            ticks.append(data[i + 2]); i += 4
        elif t == 2:
            i += 1
        elif t == 1 and i + 2 < len(data):
            hdr_hz = data[i + 1] | (data[i + 2] << 8); i += 3
        else:
            i += 1
    return ticks, hdr_hz


def capture(serial, seconds, mv, hz, csv_path=None, json_path=None, quiet=False, power_cycle=False, isolate=False):
    p = Probe(serial, et=True)
    try:
        info = p.info()
        if info["et_rc"] != 0:
            raise RuntimeError(f"probe {serial} reports no EnergyTrace ({rc_text(info['et_rc'])})")
        steps = []
        if power_cycle:
            steps.append(("power down target", p.ET_DCDC_PowerDownMCU(p.h))); time.sleep(1.0)
            steps.append(("restart DC-DC controller", p.ET_DCDC_RestartMCU(p.h))); time.sleep(0.5)
        if isolate:
            steps.append(("isolate JTAG/SWD pins", p.XDS_JTAGIsolate(p.h, 1)))
        steps += [("set target supply %d mV" % mv, p.ET_DCDC_SetVcc(p.h, mv)), ("range 0", p.ET_Setup_Range(p.h, 0)),
                 ("digital mode 2", p.ET_Setup_Dig(p.h, 2)), ("setup %d Hz" % hz, p.ET_Setup(p.h, 0, hz, 0, 0))]
        for name, rc in steps:
            if not quiet: print(f"{name}: {rc_text(rc)}", file=sys.stderr)
        # Calibration: the probe measures its own tick rate with no load (code 0) and with its 2200 ohm load (code 1).
        cal = {}
        for code in (1, 0):
            t = C.c_uint32(); n = C.c_uint32(); rc = p.ET_Calibrate(p.h, code, C.byref(t), C.byref(n))
            if rc != 0: raise RuntimeError(f"calibration {code} failed: {rc_text(rc)}")
            cal[code] = (t.value, n.value)
        norm = lambda tk, n: tk / (1000.0 * (n / hz))        # ticks per millisecond, as CCStudio's library computes it
        thr0, thr1 = norm(*cal[0]), norm(*cal[1]); i1 = mv / 2200.0 * 1e6   # nA through the 2200 ohm load
        grad = i1 / (thr1 - thr0)                                 # nA per (tick per ms)
        rc = p.ET_Start(p.h)
        if rc != 0: raise RuntimeError(f"ET_Start failed: {rc_text(rc)}")
        buf = C.create_string_buffer(0x186a0); n = C.c_int(); chunks = []; t0 = time.time()
        calls = errs = empties = 0; last_data = t0; rcs = {}; log = []
        while time.time() - t0 < seconds:
            rc = p.XDS_ReadDataPort(p.h, buf, len(buf), C.byref(n)); calls += 1
            if rc == 0 and n.value > 0:
                chunks.append(buf.raw[:n.value]); last_data = time.time(); log.append((round(last_data - t0, 3), n.value))
            elif rc != 0:
                errs += 1; rcs[rc] = rcs.get(rc, 0) + 1
                if time.time() - last_data > 2.0:   # the stream died: try to restart it rather than wait out the clock
                    p.ET_Stop(p.h); p.ET_Start(p.h); last_data = time.time(); log.append((round(last_data - t0, 3), "restart"))
            else:
                empties += 1
        p.ET_Stop(p.h); p.ET_Cleanup(p.h)
        readstats = {"calls": calls, "errors": errs, "error_codes": rcs, "empty_reads": empties, "chunks": len(chunks),
                     "last_data_s": round(last_data - t0, 3), "log_tail": log[-8:]}
    finally:
        p.close()
    data = b"".join(chunks); ticks, hdr_hz = decode(data)
    rate = hdr_hz or hz
    ma = [((tk * rate / 1000.0) - thr0) * grad / 1e6 for tk in ticks]
    dt_ms = 1000.0 / rate
    energy_uj = sum(c * mv / 1000.0 * dt_ms for c in ma)   # mA * V * ms = uJ
    summary = {"serial": serial, "probe": info, "mv": mv, "hz": rate, "seconds_requested": seconds, "read": readstats,
               "samples": len(ticks), "seconds_captured": len(ticks) * dt_ms / 1000.0,
               "calibration": {"open_ticks_per_ms": thr0, "load2200_ticks_per_ms": thr1, "na_per_tick_per_ms": grad, "raw": cal},
               "mean_mA": statistics.mean(ma) if ma else None, "min_mA": min(ma) if ma else None, "max_mA": max(ma) if ma else None,
               "energy_uJ": energy_uj, "bytes": len(data)}
    if csv_path:
        with open(csv_path, "w") as f:
            f.write("t_ms,ticks,current_mA\n")
            for k, (tk, c) in enumerate(zip(ticks, ma)):
                f.write(f"{k * dt_ms:.3f},{tk},{c:.4f}\n")
    if json_path:
        with open(json_path, "w") as f: json.dump(summary, f, indent=1)
    return summary


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--list", action="store_true", help="list attached XDS110 probes and whether they have EnergyTrace")
    ap.add_argument("--serial", help="probe serial number, as in --list")
    ap.add_argument("--seconds", type=float, default=5.0); ap.add_argument("--mv", type=int, default=3300, help="target supply from the probe, mV")
    ap.add_argument("--hz", type=int, default=2000, help="sampling rate; 2000 is the ET-MSP probe's native rate")
    ap.add_argument("--csv"); ap.add_argument("--json"); ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--power-cycle", action="store_true", help="cut and restore the probe supply first, so the target starts from a power-on reset with no debug session")
    ap.add_argument("--isolate", action="store_true", help="float the probe's SWD pins during the capture")
    a = ap.parse_args()
    if a.list: return list_probes()
    if not a.serial: ap.error("--serial or --list")
    s = capture(a.serial, a.seconds, a.mv, a.hz, os.path.abspath(a.csv) if a.csv else None, os.path.abspath(a.json) if a.json else None, a.quiet, a.power_cycle, a.isolate)
    print(f"{s['serial']}: {s['samples']} samples over {s['seconds_captured']:.2f} s at {s['hz']} Hz, {s['mv']} mV")
    print(f"current: mean {s['mean_mA']:.3f} mA, min {s['min_mA']:.3f}, max {s['max_mA']:.3f}; energy {s['energy_uJ'] / 1000:.2f} mJ")
    print(f"calibration: open {s['calibration']['open_ticks_per_ms']:.3f} ticks/ms, 2200 ohm {s['calibration']['load2200_ticks_per_ms']:.3f} ticks/ms")


if __name__ == "__main__":
    main()
