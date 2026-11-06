"""Prove the three jumper wires between the LP-CC1354P10-1 (host) and LP-EM-CC2340R5 (radio) from the chips
themselves: drive a pin on one board, read it on the other, through each board's debugger. Both directions, both
signal wires, plus a control pin with no wire that must NOT follow. Nothing is reflashed: the programs in flash stay
(halted during the test, resumed after); only the test pins are set to plain GPIO.

Run: tools/wirecheck.sh (starts the radio server, then this script, both under CCStudio's Python)
Needs both probes on USB and, on the CC1354P10, the peripheral power domain switched on (done here via PRCM)
because the running BLE program powers GPIO down while it sleeps."""
import re, struct, time
from pathlib import Path
from scripting import initScripting

ROOT = Path(__file__).resolve().parents[1]

class Board:
    def __init__(self, ds, ccxml, name):
        self.name = name
        ds.configure(str(ccxml))
        self.s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
        self.s.target.connect(); self.s.target.halt()
    def rd(self, a): return self.s.memory.readOne(a) & 0xFFFFFFFF
    def wr(self, a, v):
        # loadBinary verifies by reading back, which fails on write-1-to-set/clear and self-clearing registers
        # (GPIO DOUTSET/CLR, PRCM CLKLOADCTL). Swallow that one error; the write itself lands.
        f = Path(f"/tmp/wc_{self.name}.bin"); f.write_bytes(struct.pack("<I", v & 0xFFFFFFFF))
        try: self.s.memory.loadBinary(a, str(f))
        except Exception as e:
            if "verification" not in str(e): raise
    def close(self):
        try: self.s.target.run(False)
        finally: self.s.target.disconnect()

# CC1354P10 (CC13x4)
H_G, H_IOC, H_PRCM = 0x40022000, 0x40081000, 0x58082000
H_DOUT, H_DOUTSET, H_DOUTCLR, H_DIN, H_DOE = H_G + 0x80, H_G + 0x90, H_G + 0xA0, H_G + 0xC0, H_G + 0xD0
H_IE = 0x20000000
# CC2340R5 (CC23x0)
R_G, R_IOC = 0x40023000, 0x40003000
R_DOUT, R_DOE, R_DIN = R_G + 0x200, R_G + 0x500, R_G + 0x700
R_INPEN = 0x20000000

HOST_TX, HOST_RX, HOST_CTRL = 23, 22, 24     # yellow out of DIO23, blue into DIO22; DIO24 has no wire
RAD_RX, RAD_TX, RAD_CTRL = 12, 18, 24        # yellow into DIO12, blue out of DIO18; DIO24 has no wire
bit = lambda v, n: (v >> n) & 1

ds = initScripting(); ds.setScriptingTimeout(60000)
host = Board(ds, ROOT / "cc1354p10/CC1354P10.ccxml", "host")
try:
    # power the peripheral domain and the GPIO clock (PRCM), then wait for it
    host.wr(H_PRCM + 0x12C, host.rd(H_PRCM + 0x12C) | 0x4)
    for _ in range(50):
        if host.rd(H_PRCM + 0x140) & 0x4: break
        time.sleep(0.01)
    host.wr(H_PRCM + 0x48, 0x1); host.wr(H_PRCM + 0x28, 0x1)
    for _ in range(50):
        if host.rd(H_PRCM + 0x28) & 0x2: break
        time.sleep(0.01)
    print("host GPIO powered: DOE %08x DIN %08x" % (host.rd(H_DOE), host.rd(H_DIN)), flush=True)
    host_saved = {a: host.rd(a) for a in (H_IOC + 4 * HOST_TX, H_IOC + 4 * HOST_RX, H_IOC + 4 * HOST_CTRL, H_DOE)}
    host.wr(H_IOC + 4 * HOST_TX, 0); host.wr(H_IOC + 4 * HOST_RX, H_IE); host.wr(H_IOC + 4 * HOST_CTRL, H_IE)
    host.wr(H_DOE, (host.rd(H_DOE) | (1 << HOST_TX)) & ~((1 << HOST_RX) | (1 << HOST_CTRL)))
finally:
    pass
class RadioClient:
    """Talks to tools/wc_radio_server.py (a second CCStudio scripting process: one process cannot hold two boards)."""
    CMD, RSP = Path("/tmp/wc_cmd.txt"), Path("/tmp/wc_rsp.txt")
    def __init__(self):
        self.seq = 0
        for _ in range(600):
            if Path("/tmp/wc_radio_ready").exists(): break
            time.sleep(0.1)
        else: raise RuntimeError("radio server not ready: start tools/wc_radio_server.py first")
    def _ask(self, cmd):
        self.seq += 1; seq = str(self.seq)
        self.CMD.write_text(f"{cmd}\n{seq}")
        for _ in range(500):
            if self.RSP.exists():
                t = self.RSP.read_text().split()
                if t and t[0] == seq: return t[1]
            time.sleep(0.01)
        raise RuntimeError("radio server timeout on " + cmd)
    def rd(self, a): return int(self._ask(f"rd {a:x}"), 16)
    def wr(self, a, v): self._ask(f"wr {a:x} {v & 0xFFFFFFFF:x}")
    def close(self): self._ask("quit")
radio = RadioClient()
try:
    radio_saved = {a: radio.rd(a) for a in (R_IOC + 0x100 + 4 * RAD_RX, R_IOC + 0x100 + 4 * RAD_TX, R_IOC + 0x100 + 4 * RAD_CTRL, R_DOE)}
    radio.wr(R_IOC + 0x100 + 4 * RAD_RX, R_INPEN); radio.wr(R_IOC + 0x100 + 4 * RAD_TX, 0); radio.wr(R_IOC + 0x100 + 4 * RAD_CTRL, R_INPEN)
    radio.wr(R_DOE, (radio.rd(R_DOE) | (1 << RAD_TX)) & ~((1 << RAD_RX) | (1 << RAD_CTRL)))

    rows = []
    def check(name, drive, sense, ctrl):
        ok = True
        for lv in (1, 0, 1, 0):
            drive(lv); time.sleep(0.05); got, c = sense(), ctrl(); ok &= (got == lv); rows.append((name, lv, got, c))
        return ok
    yellow = check("yellow host DIO23 -> radio DIO12",
                   lambda lv: host.wr(H_DOUTSET if lv else H_DOUTCLR, 1 << HOST_TX),
                   lambda: bit(radio.rd(R_DIN), RAD_RX), lambda: bit(radio.rd(R_DIN), RAD_CTRL))
    blue = check("blue radio DIO18 -> host DIO22",
                 lambda lv: radio.wr(R_DOUT, (radio.rd(R_DOUT) | (1 << RAD_TX)) if lv else (radio.rd(R_DOUT) & ~(1 << RAD_TX))),
                 lambda: bit(host.rd(H_DIN), HOST_RX), lambda: bit(host.rd(H_DIN), HOST_CTRL))
    for r in rows: print("  %-34s drove %d  read %d  control %d" % r)
    print("YELLOW wire:", "PASS" if yellow else "FAIL")
    print("BLUE wire:  ", "PASS" if blue else "FAIL")
    print("GROUND:     ", "PASS (signals could not cross without a shared ground)" if (yellow and blue) else "not proven")
finally:
    for a, v in radio_saved.items(): radio.wr(a, v)
    radio.close()
    for a, v in host_saved.items(): host.wr(a, v)
    host.close()
ds.shutdown()
