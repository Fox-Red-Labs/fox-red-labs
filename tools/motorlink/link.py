#!/usr/bin/env python3
"""motorlink: a host client for TI's MotorVision UART protocol, as shipped in the AM13E2x SDK 26.01.00.03
(source/realtimelib/utilities/motorvision, TI Text File license: use with TI devices only). This file is our own
code, written from that public source; it is not TI software and not affiliated with TI.

Packets are a start byte (0x73), a command byte, the data, and an end byte (0x65). The host polls variables by
their index in the solution's motorvision_hash_variables.json and writes RW variables by index.

  python3 tools/motorlink/link.py poll                    read state, faults, bus, currents for 2 s
  python3 tools/motorlink/link.py poll --names speed_Hz VdcBus_V --secs 5
"""
import math, argparse, fcntl, json, os, pathlib, re, select, struct, termios, time

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARS = ROOT / "am13e230/foc-drv8376/motorvision_config/motorvision_hash_variables.json"
PORT = "/dev/cu.usbmodemAL1300011"
BAUD = 9200000
START, END = 0x73, 0x65
INITCMD, WRITE_VAR, STOP_POLLING, POLLING_DATA, START_POLLING_1, PACKET_ERROR = 40, 43, 76, 77, 63, 39


class LinkLost(RuntimeError):
    pass
SIZE = {"bool": 2, "uint16": 2, "int16": 2, "uint32": 4, "int32": 4, "float": 4}
FMT = {"bool": "<H", "uint16": "<H", "int16": "<h", "uint32": "<I", "int32": "<i", "float": "<f"}
RUN_FLAG = "motorVars_M1.inputCtrl.flagEnableRunAndIdentify"
# TI's table lists these enums as uint16; on this build they appear to be one byte (svmMode read 256 = svmMode 0 with
# brakingMode 1 next to it). Reads keep the low byte; writes are refused. Confirm with a debugger before relaxing this.
ENUM_ONE_BYTE = {"motorVars_M1.inputCtrl.svmMode", "motorVars_M1.inputCtrl.startupMode",
                 "motorVars_M1.inputCtrl.estimatorMode", "motorVars_M1.outVars.motorState"}

# TI's fault word (motor_common.h, FAULT_MTR_BITS), bit by bit
FAULT_BITS = ["DC bus over voltage", "DC bus under voltage", "motor over temperature", "power module over temperature",
              "hardware over current", "comparator over current", "over load", "motor lost phase",
              "phase current unbalance", "motor stall", "startup failed", "over speed",
              "estimator error", "gate driver fault", "current offset check", "voltage offset check"]
# TI's motor state (est_lib motor.h, MOTOR_Status_e)
STATES = {0: "stop idle", 1: "fault stop", 2: "brake stop", 3: "normal stop", 4: "charge", 5: "seek position",
          6: "alignment", 7: "parameter identification", 8: "initial position detection", 9: "open-loop start",
          10: "open-loop end", 11: "closed loop running", 12: "control run", 13: "field weakening run",
          14: "brake run", 15: "motor ID start", 16: "motor ID run", 17: "motor ID done", 18: "Rs calibration start", 19: "Rs calibration run",
          20: "Rs calibration done", 21: "parameters store", 22: "code update", 23: "offset calibration", 24: "Hall calibration", 25: "Hall startup"}


TABLE = VARS.with_name("motorvision_hash.c")


def load_vars(path=VARS, table=TABLE):
    """Index and type from the firmware's own table (motorvision_hash.c), permissions from the JSON by name.
    In SDK 26.01.00.03 the two disagree in every motor solution: the C table lists VdcBus_V three times (45 entries,
    JSON 43), so from index 34 the JSON's indices are two short. The firmware answers by the C table."""
    perms = {k: v["permissions"] for k, v in json.loads(pathlib.Path(path).read_text()).items()}
    src = pathlib.Path(table).read_text()
    starts = {t: int(n) for t, n in re.findall(r"MVHASH_(\w+?)(?:_t)?_startIndex\s*=\s*(-?\d+)", src)}
    body = src[src.index("MVHASH_hash_table"):]
    names = re.findall(r"&([\w.\[\]]+)\s*,", body[:body.index("};")])
    bounds = sorted((i, t) for t, i in starts.items() if i >= 0)
    out = {}
    for idx, name in enumerate(names):
        t = [t for i, t in bounds if i <= idx][-1]
        t = {"uint16": "uint16", "uint32": "uint32", "int16": "int16", "int32": "int32", "bool": "bool", "float": "float"}[t]
        if name not in out:  # a repeated entry keeps its first index; the repeats still answer, with the same value
            out[name] = {"index": idx, "type": t, "permissions": perms.get(name, "R"), "name": name}
    return out


def find(vars_, short):
    """a full name, or a unique ending such as speed_Hz or VdcBus_V"""
    if short in vars_:
        return vars_[short]
    hits = [v for k, v in vars_.items() if k.endswith(short) or k.endswith("." + short)]
    if len(hits) != 1:
        raise SystemExit(f"{short}: {'no' if not hits else 'more than one'} variable matches; names are in {VARS.name}")
    return hits[0]


def guard_version():
    m = re.search(r"#define FRL_GUARD_VERSION\s+(\d+)U", (TABLE.parent.parent / "frl_guard.h").read_text())
    return int(m.group(1)) if m else None


def hall_adj_default(table=0):
    """the Hall offset the guard boots with (FRL_GUARD_HALL_ADJ_DEG, revision H): tools put frlGuard.angleAdj_deg back to
    this after a sweep, not to 0. table=1: the offset for the calibrated Hall table (frlGuard.hallTable 1,
    FRL_GUARD_HALL_ADJ_CAL_DEG, revision H4), which the guard loads when hallTable is written"""
    name = "FRL_GUARD_HALL_ADJ_CAL_DEG" if table else "FRL_GUARD_HALL_ADJ_DEG"
    m = re.search(r"#define " + name + r"\s+(-?[\d.]+)f", (TABLE.parent.parent / "frl_guard.h").read_text())
    return float(m.group(1)) if m else 0.0


def decode_faults(word):
    return [FAULT_BITS[b] for b in range(16) if word >> b & 1]


class Link:
    def __init__(self, port=PORT, baud=BAUD, vars_path=VARS):
        self.vars = load_vars(vars_path)
        self.by_index = {v["index"]: v for v in self.vars.values()}
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, getattr(termios, 'TIOCEXCL', 0x2000740d))   # exclusive: a browser or second tool gets 'busy' at once instead of sharing the byte stream
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[4] = a[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        fcntl.ioctl(self.fd, 0x80085402, struct.pack("L", baud))  # IOSSIOSPEED: a non-standard rate on macOS
        time.sleep(0.2); termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.buf = b""
        self.resyncs = 0
        self.synced_after = self.sync()

    def close(self):
        os.close(self.fd)

    def send(self, cmd, payload=b""):
        if len(payload) > 13:
            raise ValueError("payload is 13 bytes at most")
        self._write_all(bytes([START, cmd]) + payload.ljust(13, b"\0") + bytes([END]))

    def _write_all(self, data):
        """the port is non-blocking, so a write can go out short; the board counts bytes into 16-byte packets and a
        lost byte would put every later packet out of step"""
        while data:
            try:
                n = os.write(self.fd, data)
                data = data[n:]
            except BlockingIOError:
                select.select([], [self.fd], [], 0.05)

    def sync(self, tries=16):
        """Bring the board's 16-byte receive counter back in step. Its receiver never looks for the start byte, so
        after a lost byte it answers every packet with PACKET_ERROR (39) until reset. Send a harmless STOP_POLLING
        probe; on an error, add one filler byte and try again. A filler window cannot frame as a packet: it starts
        with 0x00, not 0x73."""
        for k in range(tries):
            self.buf = b""
            self.send(STOP_POLLING); self.read(0.08)
            if bytes([START, STOP_POLLING, END]) in self.buf:
                self.buf = b""
                return k
            self._write_all(b"\0"); self.read(0.02)
        raise SystemExit("could not get in step with the board's receiver; reset the board")

    def read(self, secs):
        t0 = time.time()
        while time.time() - t0 < secs:
            r, _, _ = select.select([self.fd], [], [], 0.02)
            if r:
                try:
                    self.buf += os.read(self.fd, 65536)
                except BlockingIOError:
                    pass

    def polled(self):
        """pull POLLING_DATA packets out of the buffer: {index: value}"""
        out, b, i = {}, self.buf, 0
        while True:
            i = b.find(bytes([START, POLLING_DATA]), i)
            if i < 0 or i + 3 > len(b):
                break
            idx = b[i + 2]
            v = self.by_index.get(idx)
            n = SIZE[v["type"]] if v else 0
            if v and i + 3 + n < len(b) and b[i + 3 + n] == END:
                val = struct.unpack(FMT[v["type"]], b[i + 3:i + 3 + n])[0]
                out[idx] = val & 0xFF if v["name"] in ENUM_ONE_BYTE else val
                i += 4 + n
            else:
                i += 1
        self.buf = b[-64:]
        return out

    def poll(self, names, secs=1.0):
        vs = [find(self.vars, n) for n in names]
        if len(vs) > 13:
            raise SystemExit("13 variables per poll at most")
        start = lambda: (self.send(STOP_POLLING), self.read(0.05),
                         self.send(START_POLLING_1 + len(vs) - 1, bytes(v["index"] for v in vs)))
        start()
        got, t0, heard, replied, retried = {}, time.time(), time.time(), time.time(), False
        while time.time() - t0 < secs:
            n0 = len(self.buf); self.read(0.1)
            if len(self.buf) > n0:
                heard = time.time()
            if bytes([START, PACKET_ERROR, END]) in self.buf:  # out of step mid-session: resync, poll again
                self.resyncs += 1; self.sync(); start(); continue
            new = self.polled()
            if new:
                replied = time.time(); got.update(new)
            if time.time() - heard > 1.0:
                raise LinkLost("no bytes from the board for 1 s: it may be halted; read its state with the debug probe before resetting")
            if time.time() - replied > 1.0:  # bytes, but no poll replies: the board stopped hearing us
                if retried:
                    raise LinkLost("the board sends but no longer answers polls: its receive path may be stuck")
                retried = True; self.resyncs += 1; self.sync(); start(); replied = time.time()
        self.send(STOP_POLLING)
        return {v["name"]: got.get(v["index"]) for v in vs}

    def board_version(self):
        """frlGuard.version, read at its fixed index (5, right after TI's five uint16 entries)"""
        if getattr(self, "_board_version", None) is None:
            self._board_version = self.poll(["frlGuard.version"], 0.5).get("frlGuard.version")
        return self._board_version

    def write(self, name, value, allow_run=False):
        v = find(self.vars, name)
        if v["name"] in ENUM_ONE_BYTE:
            raise SystemExit(f"{v['name']} is an enum TI's table lists as uint16, but it appears to be one byte on this build; "
                             "a write would also change the next field. Not written.")
        want = guard_version()
        if self.board_version() != want:
            raise SystemExit(f"the board's guard version is {self.board_version()!r} but this client's table is for {want}: "
                             "a write would land on the wrong variable; flash the matching image or check out the matching commit")
        if "W" not in v["permissions"]:
            raise SystemExit(f"{v['name']} is read-only")
        if v["name"] == RUN_FLAG and value and not allow_run:
            raise SystemExit("starting the motor needs allow_run=True; the caller owns the bench guards")
        t = v["type"]
        if t == "bool":
            data = bytes([0, 1 if value else 0])
        else:
            if t == "float" and not math.isfinite(float(value)):   # never send NaN or infinity to the board (guard suite, 2026-10-19)
                raise ValueError(f"refusing to write a non-finite value to {name}")
            data = struct.pack(">" + FMT[t][1], int(value) if t != "float" else float(value))
        self.send(WRITE_VAR, bytes([v["index"]]) + data)
        self.read(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["poll"])
    ap.add_argument("--names", nargs="*", default=["motorState", "faultMtrNow", "ISRCount", "VdcBus_V", "speed_Hz",
                                                   "I_A.value[0]", "I_A.value[1]", "I_A.value[2]"])
    ap.add_argument("--secs", type=float, default=2.0)
    a = ap.parse_args()
    link = Link()
    try:
        vals = link.poll(a.names, a.secs)
    finally:
        link.close()
    for k, v in vals.items():
        note = ""
        if k.endswith("motorState") and v is not None: note = f"  ({STATES.get(v, 'unknown')})"
        if k.endswith("faultMtrNow") and v is not None: note = f"  ({', '.join(decode_faults(v)) or 'no fault'})"
        print(f"{k:45s} {v!r}{note}")


if __name__ == "__main__":
    main()
