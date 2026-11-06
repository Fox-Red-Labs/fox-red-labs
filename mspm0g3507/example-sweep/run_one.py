#!/usr/bin/env python3
"""Flash one built MSPM0 SDK example to the LP-MSPM0G3507 on probe MG350001, let it run,
and record what it did. Writes one JSON line to <results>/runs.jsonl.

  run_one.py <example .out> <results dir> [run_seconds]

What it records, all measured on the board:
  - safety: the image is refused if any loadable segment falls outside MAIN flash or SRAM
    (NONMAIN at 0x41C00000 holds the boot configuration; a bad write there can lock a board)
  - flash: OpenOCD program + verify time and result
  - uart: every byte on the XDS110 backchannel (PA10/PA11) for the run, at the example's
    own baud rate when its generated config names one, else 115200
  - pins: GPIOA and GPIOB output registers sampled 8 times, 250 ms apart, while running
  - state: after the run, halt and read PC, LR and xPSR; the PC is mapped to a function name
"""
import json, os, re, select, subprocess, sys, termios, threading, time
from typing import Any

PROBE = "MG350001"
TTY = f"/dev/cu.usbmodem{PROBE}1"
CGT = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin"
OCD = os.path.expanduser("~/zephyr-toolchain/xpack-openocd-0.12.0-7")
OCD_CFG = os.path.expanduser("~/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg")
GPIOA_DOUT, GPIOB_DOUT = 0x400A1280, 0x400A3280
GPIOA_DOE, GPIOB_DOE = 0x400A12C0, 0x400A32C0
# LaunchPad LEDs: LED1 = PA0 (active low), RGB LED2 = PB22 blue, PB26 red, PB27 green
LED_BITS = {"PA0": ("A", 0), "PB22": ("B", 22), "PB26": ("B", 26), "PB27": ("B", 27)}
SAFE = [(0x00000000, 0x00020000), (0x20000000, 0x20008000), (0x20100000, 0x20108000)]
BAUDS = {9600: termios.B9600, 19200: termios.B19200, 38400: termios.B38400,
         57600: termios.B57600, 115200: termios.B115200, 230400: termios.B230400}


def segments(out):
    r = subprocess.run([f"{CGT}/tiarmreadelf", "-l", "-W", out], capture_output=True, text=True).stdout
    segs = []
    for line in r.splitlines():
        p = line.split()
        if p and p[0] == "LOAD":
            phys, filesz = int(p[3], 16), int(p[4], 16)
            if filesz:
                segs.append((phys, filesz))
    return segs


def is_safe(segs):
    return all(any(lo <= a and a + n <= hi for lo, hi in SAFE) for a, n in segs)


def symbols(out):
    r = subprocess.run([f"{CGT}/tiarmnm", "-n", "--defined-only", out], capture_output=True, text=True).stdout
    syms = []
    for line in r.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tTwW":
            syms.append((int(p[0], 16) & ~1, p[2]))
    return syms


def where(syms, pc):
    best = None
    for a, n in syms:
        if a <= pc:
            best = (a, n)
        else:
            break
    return f"{best[1]}+0x{pc - best[0]:x}" if best else None


def bkpt_at(out, addr):
    d = subprocess.run([f"{CGT}/tiarmobjdump", "-d", f"--start-address=0x{addr:x}", f"--stop-address=0x{addr + 2:x}", out],
                       capture_output=True, text=True).stdout
    return "bkpt" in d


def uart_config(outdir):
    h = os.path.join(outdir, "ti_msp_dl_config.h")
    if not os.path.exists(h):
        return None, None
    t = open(h, errors="replace").read()
    inst = re.findall(r"#define\s+(UART_\w+?)_INST\s+(UART\d)\b", t)
    for name, hw in inst:
        if hw == "UART0":  # UART0 on PA10/PA11 is the LaunchPad backchannel
            m = re.search(rf"#define\s+{name}_BAUD_RATE\s+\(?(\d+)", t)
            return hw, int(m.group(1)) if m else None
    return (inst[0][1] if inst else None), None


class Uart(threading.Thread):
    def __init__(self, baud):
        super().__init__(daemon=True)
        self.buf, self.stop, self.baud = b"", False, baud

    def run(self):
        try:
            fd = os.open(TTY, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
        except OSError as e:
            self.buf = f"[uart open failed: {e}]".encode()
            return
        a = termios.tcgetattr(fd)
        a[4] = a[5] = BAUDS.get(self.baud, termios.B115200)
        a[2] |= termios.CLOCAL | termios.CREAD
        a[3] = a[0] = a[1] = 0
        termios.tcsetattr(fd, termios.TCSANOW, a)
        termios.tcflush(fd, termios.TCIFLUSH)
        while not self.stop:
            if select.select([fd], [], [], 0.2)[0]:
                try:
                    self.buf += os.read(fd, 4096)
                except OSError:
                    pass
        os.close(fd)


def ocd(cmds, timeout):
    args = [f"{OCD}/bin/openocd", "-s", f"{OCD}/openocd/scripts", "-f", OCD_CFG, "-c", f"adapter serial {PROBE}", "-c", "init"]
    for c in cmds:
        args += ["-c", c]
    args += ["-c", "shutdown"]
    t0 = time.time()
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return p.stdout + p.stderr, time.time() - t0, p.returncode
    except subprocess.TimeoutExpired as e:
        return (e.stdout or b"").decode(errors="replace") + "[TIMEOUT]", time.time() - t0, -9


def main():
    out, resdir = sys.argv[1], sys.argv[2]
    run_s = float(sys.argv[3]) if len(sys.argv) > 3 else 4
    rec: dict[str, Any] = {"out": out, "id": out.split("/examples/")[-1].rsplit("/ticlang/", 1)[0].replace("/", "_")}
    segs = segments(out)
    rec["segments"] = [[hex(a), n] for a, n in segs]
    if not is_safe(segs):
        rec["verdict"] = "skipped_unsafe_segment"
        print(json.dumps(rec))
        with open(os.path.join(resdir, "runs.jsonl"), "a") as f:
            f.write(json.dumps(rec) + "\n")
        return
    uart_inst, baud = uart_config(os.path.dirname(out))
    rec["uart_instance"], rec["uart_baud"] = uart_inst, baud
    # Session 1: halt the old program and write the new one, leaving the core halted,
    # so nothing the old program printed is counted.
    # Halt alone is not enough: a running DMA keeps writing SRAM, where the flash and verify
    # routines run. A halted reset stops every peripheral first.
    flog, fsecs, frc = ocd(["reset halt", f"program {{{out}}} verify"], timeout=90)
    rec["flash_attempts"] = 1
    if "** Verified OK **" not in flog:
        # The previous example can leave the core hard to halt (watchdog, low power, busy DMA).
        # Retry once after a pin reset (NRST), the way an engineer would.
        flog2, fsecs2, frc = ocd(["mspm0_board_reset", "halt", f"program {{{out}}} verify"], timeout=90)
        flog, fsecs = flog + "\n===== RETRY (board reset, halt) =====\n" + flog2, fsecs + fsecs2
        rec["flash_attempts"] = 2
        if "** Verified OK **" in flog2:
            flog = flog.replace("** Verified OK **", "") + "\n** Verified OK **"
    u = Uart(baud or 115200)
    u.start()
    time.sleep(0.3)
    samples = []
    for _ in range(8):
        samples += [f"mdw 0x{GPIOA_DOUT:08x}", f"mdw 0x{GPIOB_DOUT:08x}", "sleep 250"]
    # Session 2: reset, run, sample the pins while running, then halt and read registers.
    # RESET=nrst uses the reset pin, like a power-up: it also resets the debug logic, so a __BKPT in
    # the example becomes a HardFault, as it would on a board with no debugger. The default is a
    # system reset request, which keeps halting debug on, so __BKPT stops the core the way the
    # example's author expected when running it from CCStudio.
    reset_cmd = "mspm0_board_reset" if os.environ.get("RESET") == "nrst" else "reset run"
    rec["reset"] = reset_cmd
    cmds = [reset_cmd, f"sleep {int(run_s * 1000)}",
            f"mdw 0x{GPIOA_DOE:08x}", f"mdw 0x{GPIOB_DOE:08x}", *samples,
            "halt", "reg pc", "reg lr", "reg xpsr", "reg sp",
            "echo STACKFRAME", "mdw [lindex [split [reg sp] { }] 2] 8"]
    log, secs, rc = ocd(cmds, timeout=90)
    u.stop = True
    u.join(1)
    rec["flash_secs"], rec["flash_rc"] = round(fsecs, 1), frc
    log = flog + "\n===== RUN =====\n" + log
    os.makedirs(os.path.join(resdir, "ocd"), exist_ok=True)
    open(os.path.join(resdir, "ocd", rec["id"] + ".log"), "w").write(log)
    rec["run_secs"], rec["run_rc"] = round(secs, 1), rc
    rec["verified"] = "** Verified OK **" in flog.split("===== RETRY")[-1]
    words = [int(w, 16) for w in re.findall(r"^0x400a[0-9a-f]{4}:\s+([0-9a-f]{8})", log, re.M)]
    if len(words) >= 2:
        doe_a, doe_b = words[0], words[1]
        a = words[2::2][:8]
        b = words[3::2][:8]
        rec["doe"] = [hex(doe_a), hex(doe_b)]
        rec["dout_changing_bits"] = {"A": hex(sum(1 << i for i in range(32) if len({(x >> i) & 1 for x in a}) > 1)),
                                     "B": hex(sum(1 << i for i in range(32) if len({(x >> i) & 1 for x in b}) > 1))}
        leds = {}
        for name, (port, bit) in LED_BITS.items():
            vals = a if port == "A" else b
            doe = doe_a if port == "A" else doe_b
            if (doe >> bit) & 1:
                st = {(x >> bit) & 1 for x in vals}
                leds[name] = "toggling" if len(st) > 1 else ("high" if 1 in st else "low")
        rec["leds"] = leds
    regs = dict(re.findall(r"^(pc|lr|xpsr|sp) \([^)]*\): (0x[0-9a-f]+)", log, re.M))
    rec["regs"] = regs
    run_part = log.split("===== RUN =====")[-1]
    halted_on_own = re.search(r"halted due to (breakpoint|watchpoint)", run_part)
    rec["halt_reason"] = halted_on_own.group(1) if halted_on_own else None
    if "pc" in regs:
        syms = symbols(out)
        pc = int(regs["pc"], 16)
        rec["pc_at"] = where(syms, pc)
        rec["lr_at"] = where(syms, int(regs.get("lr", "0"), 16) & ~1)
        ipsr = int(regs.get("xpsr", "0"), 16) & 0x3F
        rec["ipsr"] = ipsr
    fr = re.search(r"STACKFRAME\s*\n(0x2[0-9a-f]{7}:(?:\s+[0-9a-f]{8})+)", log)
    if fr and rec.get("ipsr", 0) >= 2:
        w = [int(x, 16) for x in fr.group(1).split(":")[1].split()]
        if len(w) >= 7:
            # Cortex-M exception frame: r0 r1 r2 r3 r12 lr pc xpsr
            rec["fault_pc"], rec["fault_lr"] = hex(w[6]), hex(w[5])
            rec["fault_pc_at"] = where(symbols(out), w[6])
    text = u.buf.decode(errors="replace").replace("\r", "")
    rec["uart_bytes"] = len(u.buf)
    rec["uart_head"] = text[:600]
    # verdict
    if not rec["verified"]:
        rec["verdict"] = "flash_failed"
    elif "pc" not in regs:
        rec["verdict"] = "no_debug_after_run"  # usually a low-power mode or the SWD pins were repurposed
    elif rec.get("ipsr") == 3 and rec.get("fault_pc") and bkpt_at(out, int(rec["fault_pc"], 16)):
        rec["verdict"] = "bkpt_became_hardfault"
    elif rec.get("ipsr") == 3 or (rec.get("pc_at") or "").startswith(("HardFault", "Default_Handler", "NMI_Handler")):
        rec["verdict"] = "fault"
    elif "breakpoint" in (rec["halt_reason"] or ""):
        rec["verdict"] = "stopped_at_breakpoint"
    else:
        rec["verdict"] = "running"
    with open(os.path.join(resdir, "runs.jsonl"), "a") as f:
        f.write(json.dumps(rec) + "\n")
    print(json.dumps({k: rec.get(k) for k in ("id", "verdict", "pc_at", "leds", "uart_bytes", "flash_secs", "run_secs")}))


if __name__ == "__main__":
    main()
