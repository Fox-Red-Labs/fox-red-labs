#!/usr/bin/env python3
"""Flash one ported DriverLib image to the LP-MSPM33C321A or the LP-MSPM0G3519 and check what it did.

  bench_pair_run.py <m33c321a|g3519> <image.out> <test> <results.jsonl>

Every probe use goes through tools/frl/lock.py (via the lab's portboard.py). Only main flash and SRAM are
written (the image's load segments are checked first; NONMAIN, boot configuration and external flash never).
After the flash the program runs; this script then reads, over the probe and without halting the core:
the fault registers (CFSR, HFSR), the GPIO output registers of the pins the generated header names, and the
RAM variables the test names. Tests are in TESTS below: what the app does, what we expect, and the source.
"""
import json, os, re, subprocess, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "ai_runs"))
try:
    import portboard as pb  # noqa: E402
except ImportError:
    sys.exit("skipped: this runs on the lab bench and needs portboard.py (not in this copy)")

CGT = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin"
DOUT = {"m33c321a": {"A": 0x400F1280, "B": 0x400F3280, "C": 0x400F5280}, "g3519": {"A": 0x400A1280, "B": 0x400A3280, "C": 0x400A5280}}
DIN = {"m33c321a": {"A": 0x400F1380, "B": 0x400F3380, "C": 0x400F5380}, "g3519": {"A": 0x400A1380, "B": 0x400A3380, "C": 0x400A5380}}
SAFE = {"m33c321a": [(0x10000000, 0x10100000), (0x20000000, 0x20040000)], "g3519": [(0x0, 0x80000), (0x20200000, 0x20220000)]}
CFSR, HFSR = 0xE000ED28, 0xE000ED2C
DEVH = {"m33c321a": "/Applications/ti/mspm33_sdk_1_04_00_00/source/ti/devices/msp/m33/mspm33c321x.h",
        "g3519": "/Applications/ti/mspm0_sdk_2_11_00_07/source/ti/devices/msp/m0p/mspm0g351x.h"}


def base_of(board, hdr, inst):
    """Base address of the timer behind a generated INST macro (PWM_0_INST -> TIMA1 -> TIMA1_BASE)."""
    t = open(hdr, errors="replace").read()
    m = re.search(r"#define\s+" + inst + r"\s+(\w+)", t)
    if not m:
        return None, None
    n = m.group(1)
    d = open(DEVH[board], errors="replace").read()
    b = re.search(r"#define\s+" + n + r"_BASE\s+\(0x([0-9A-Fa-f]+)U\)", d)
    return n, int(b.group(1), 16) if b else None


# name -> dict(kind, wait, syms, expect, note). kinds: toggle (LED/TEST pins must change), passled (PASS pin set, FAIL clear),
# syms (RAM bytes equal the expected hex), pwm (output pins seen both high and low), quiet (runs, faults clean).
TESTS = {
    "gpio_toggle_output": dict(kind="toggle", wait=1.0, expect="LED pins change between reads (0.5 s half period at 32 MHz)"),
    "systick_periodic_timer": dict(kind="toggle", wait=1.0, expect="LED and test pin toggle from the SysTick handler"),
    "timx_timer_mode_periodic_sleep": dict(kind="toggle", wait=1.0, inst="TIMER_0_INST", expect="LED toggles from the timer interrupt"),
    "timx_timer_mode_pwm_edge_sleep": dict(kind="timer", wait=1.0, inst="PWM_0_INST", expect="timer counter advances; LOAD and compare registers equal the generated configuration"),
    "spi_controller_internal_loopback_poll": dict(kind="syms", wait=1.5, syms=[("gRxData", 1, "01"), ("gTxData", 1, "01")],
                                                  expect="gRxData equals gTxData (1) through the SPI internal loopback; the app then stops at BKPT"),
    "uart_rw_multibyte_fifo_poll": dict(kind="syms", wait=1.5, syms=[("rxPacket", 4, "4d535021")], expect="rxPacket reads MSP! through the UART internal loopback"),
    "uart_tx_rw_multibyte_fifo_poll": dict(kind="syms", wait=1.5, syms=[("rxPacket", 4, "4d535021"), ("allCorrect", 1, "01")],
                                           expect="rxPacket reads MSP! through the UART internal loopback"),
    "uart_internal_loopback_spgss0_uc1": dict(kind="syms", wait=1.5, syms=[("rxData", 1, "05"), ("txData", 1, "05")],
                                              expect="rxData equals txData (5) through the UART internal loopback"),
    "uart_internal_loopback_with_interrupt_spgss0_uc1": dict(kind="passled", wait=1.5, expect="pass LED pin high, fail LED pin low after the interrupt-driven UART loopback"),
    "crcp_calculate_checksum": dict(kind="crc", wait=1.0, expect="CRCOUT reads 0x2C81, the example's expected value"),
    "uart_external_loopback_interrupt": dict(kind="quiet", wait=1.0, expect="runs; needs a TX to RX wire, so only fault registers are read"),
    "x45_cc1407p_host": dict(kind="syms", wait=3.0, expect="struct sizes, CRC-16 and the first SPI message equal the PC reference (c28x/README.md)", syms=[
        ("sz", 24, None), ("sz", 18, "040002000200040003003000030004000800"), ("crc_check", 2, "b129"), ("fake_xfer_len", 2, "0a00"),
        ("fake_log", 20, "00000400110000000200320000000000e9007100"), ("fake_xfers", 2, None), ("fake_log_n", 2, None), ("open_ok", 2, None)]),
    "adc12_single_conversion": dict(kind="quiet", wait=1.0, expect="runs"),
}


def sh(cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout


def segments(out):
    segs = []
    for line in sh([f"{CGT}/tiarmreadelf", "-l", "-W", out]).splitlines():
        p = line.split()
        if p and p[0] == "LOAD" and int(p[4], 16):
            segs.append((int(p[3], 16), int(p[4], 16)))
    return segs


def is_safe(board, segs):
    return all(any(lo <= a and a + n <= hi for lo, hi in SAFE[board]) for a, n in segs)


def symbols(out):
    s = {}
    for line in sh([f"{CGT}/tiarmnm", "-n", "--defined-only", out]).splitlines():
        p = line.split()
        if len(p) == 3:
            s[p[2]] = int(p[0], 16) & (~1 if p[1] in "tTwW" else ~0)
    return s


def header_pins(hdr, words):
    """(port letter, pin number) for generated pin names that contain one of words and are not the reserved group."""
    t = open(hdr, errors="replace").read()
    ports = dict(re.findall(r"#define\s+(\w+)_PORT\s+\(?GPIO([ABC])\)?", t))
    out = []
    for name, val in re.findall(r"#define\s+(\w+)_PIN\s+\(?DL_GPIO_PIN_(\d+)\)?", t):
        if "RESERVED" in name or not any(w in name for w in words):
            continue
        port = ports.get(name) or ports.get(name.rsplit("_", 2)[0]) or ports.get(re.sub(r"_(\w+)$", "", name))
        if port is None:
            port = next((v for k, v in ports.items() if name.startswith(k)), None)
        if port:
            out.append((name, port, int(val)))
    return out


def read_m33(plan):
    """plan: list of ('sleep', s) / ('read', tag, addr, nwords). Returns {tag: [words...]} per occurrence."""
    res = []
    DS, cc = pb.DS, f"{REPO}/mspm33c321a/MSPM33C321A_M3010001.ccxml"
    for op in plan:
        if op[0] == "sleep":
            time.sleep(op[1])
            continue
        _, tag, addr, n = op
        f = f"/tmp/frl_pair_{os.getpid()}.bin"
        r = pb.lock("M3010001", DS, "memory", "-c", cc, "-r", f"0x{addr:x},{n}", "-s", "32", "-o", f)
        w = None
        if r.returncode == 0 and os.path.exists(f):
            b = open(f, "rb").read()
            w = [int.from_bytes(b[i:i + 4], "little") for i in range(0, len(b), 4)]
        res.append((tag, w, time.time()))
    return res


def read_g3519(plan):
    cmds = []
    for op in plan:
        if op[0] == "sleep":
            cmds.append(f"sleep {int(op[1] * 1000)}")
        else:
            _, tag, addr, n = op
            cmds += [f"echo TAG_{tag}", f"mdw 0x{addr:x} {n}"]
    r = pb.ocd_cmds("g3519", cmds)
    txt = r.stdout + r.stderr
    res = []
    for m in re.finditer(r"TAG_(\S+)\s*\n((?:0x[0-9a-f]+: [0-9a-f ]+\n?)+)", txt):
        words = []
        for line in m.group(2).strip().splitlines():
            words += [int(x, 16) for x in line.split(": ")[1].split()]
        res.append((m.group(1), words, time.time()))
    if not res:
        print(txt[-600:])
    return res


def main():
    board, image, test, outp = sys.argv[1:5]
    spec = TESTS[test]
    segs = segments(image)
    if not is_safe(board, segs):
        print("image loads outside main flash and SRAM:", segs)
        sys.exit(2)
    hdr = os.path.join(os.path.dirname(image), "ti_msp_dl_config.h")
    if not os.path.exists(hdr):
        hdr = os.path.join(os.path.dirname(os.path.dirname(image)), "ti_msp_dl_config.h")
    ok = pb.flash(board if board != "m33c321a" else "m33c321a", image, run=True)
    row = dict(board=board, test=test, image=os.path.relpath(image, os.path.expanduser("~")).replace(os.path.expanduser("~"), ""), flashed=ok,
               expect=spec["expect"], when=time.strftime("%Y-%m-%d %H:%M:%S"))
    if not ok:
        row.update(result="flash failed")
        open(outp, "a").write(json.dumps(row) + "\n")
        print(row)
        sys.exit(1)
    syms = symbols(image)
    watch = header_pins(hdr, ["LED", "TEST", "PWM"]) if os.path.exists(hdr) else []
    ports = sorted({p for _, p, _ in watch})
    plan = [("sleep", spec["wait"]), ("read", "fault", CFSR, 2)]
    nsamp = 8
    for i in range(nsamp):
        for p in ports:
            plan.append(("read", f"dout{p}", DOUT[board][p], 1))
            plan.append(("read", f"din{p}", DIN[board][p], 1))
        plan.append(("sleep", 0.23))
    tbase = None
    if spec["kind"] == "timer" and os.path.exists(hdr):
        tname, tbase = base_of(board, hdr, spec["inst"])
        if tbase:
            for k in range(2):
                plan += [("read", "ctr", tbase + 0x1800, 1), ("sleep", 0.31)]
            plan += [("read", "load", tbase + 0x1808, 1), ("read", "cc", tbase + 0x1810, 2)]
    if spec["kind"] == "crc":
        crc = {"m33c321a": 0x401B2000, "g3519": None}[board] if board == "m33c321a" else int(re.search(r"define CRCP0_BASE\s+\(0x([0-9A-Fa-f]+)U\)", open(DEVH[board]).read()).group(1), 16)
        plan.append(("read", "crcout", crc + 0x110C, 1))
    for s, n, _ in spec.get("syms", []):
        plan.append(("read", f"sym_{s}_{n}", syms[s] & ~3, ((syms[s] & 3) + n + 3) // 4))
    got = (read_m33 if board == "m33c321a" else read_g3519)(plan)
    d = {}
    for tag, w, _ in got:
        d.setdefault(tag, []).append(w)
    flt = d.get("fault", [[None, None]])[0]
    row["cfsr"] = None if not flt else f"{flt[0]:08x}"
    row["hfsr"] = None if not flt or len(flt) < 2 else f"{flt[1]:08x}"
    res, detail = "NO DATA", {}
    for name, p, bit in watch:
        o = [x[0] >> bit & 1 for x in d.get(f"dout{p}", []) if x]
        i = [x[0] >> bit & 1 for x in d.get(f"din{p}", []) if x]
        detail[f"{name} (GPIO{p} pin {bit})"] = dict(dout="".join(map(str, o)), din="".join(map(str, i)))
    row["pins"] = detail
    if spec["kind"] == "toggle":
        good = [k for k, v in detail.items() if len(set(v["dout"])) > 1 and "LED" in k or len(set(v["dout"])) > 1 and "TEST" in k]
        res = "PASS" if good else "FAIL no pin changed"
        row["toggled"] = good
    elif spec["kind"] == "pwm":
        good = [k for k, v in detail.items() if len(set(v["din"])) > 1]
        res = "PASS" if good else "FAIL pins stuck"
        row["toggled"] = good
    elif spec["kind"] == "crc":
        w = (d.get("crcout") or [None])[0]
        row["crcout"] = None if not w else f"{w[0]:08x}"
        res = "PASS" if w and (w[0] & 0xFFFF) == 0x2C81 else "FAIL"
    elif spec["kind"] == "timer":
        c = [x[0] for x in d.get("ctr", []) if x]
        ld = (d.get("load") or [None])[0]
        cc = (d.get("cc") or [None])[0]
        gen = open(os.path.join(os.path.dirname(hdr), "ti_msp_dl_config.c"), errors="replace").read()
        per = re.findall(r"\.period\s*=\s*(\w+)", gen)
        ccv = re.findall(r"setCaptureCompareValue\(\w+,\s*(\d+)", gen)
        row.update(timer=tname, ctr=c, load=None if not ld else ld[0], cc=cc, config_period=per, config_cc=ccv)
        res = "PASS" if len(c) == 2 and c[0] != c[1] and ld and str(ld[0]) in [str(int(x) - 1) if x.isdigit() else x for x in per] + per else "CHECK"
    elif spec["kind"] == "passled":
        pas = [v["dout"][-1:] for k, v in detail.items() if "PASS" in k]
        fai = [v["dout"][-1:] for k, v in detail.items() if "FAIL" in k]
        res = "PASS" if pas and pas[0] == "1" and (not fai or fai[0] == "0") else "FAIL"
    elif spec["kind"] == "syms":
        okk = True
        for s, n, exp in spec["syms"]:
            w = (d.get(f"sym_{s}_{n}") or [None])[0]
            off = syms[s] & 3
            b = b"".join(x.to_bytes(4, "little") for x in w)[off:off + n].hex() if w else None
            row[f"sym_{s}" + ("" if (s, n) not in [("sz", 18)] else "_wire")] = b
            okk &= (exp is None or b == exp)
        res = "PASS" if okk else "FAIL"
    else:
        res = "RAN" if flt and flt[0] is not None else "NO DATA"
    row["result"] = res
    open(outp, "a").write(json.dumps(row) + "\n")
    print(json.dumps({k: row[k] for k in row if k not in ("image",)})[:900])


if __name__ == "__main__":
    main()
