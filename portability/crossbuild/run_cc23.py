#!/usr/bin/env python3
"""Run ported MSPM0 apps on the LP-EM-CC2340R5 next to TI's own CC2340R5 version of each, then
put the board back exactly as it was.

Steps: (1) read back the whole flash and CCFG to a backup outside the repo (it holds the Zigbee
network key), (2) for each app, flash TI's native build, then the ported build, each time capturing
the backchannel UART for a few seconds and reading the GPIO output and output-enable registers,
(3) flash the backup back. Writes one JSON line per run.

Usage: run_cc23.py <images dir> <backup dir> <results jsonl> [seconds]
<images dir> holds <app>.native.out and <app>.ported.out.
Probe LS470GTU only; the .ccxml selects it by serial, so the G3507's probe is never touched."""
import json, os, select, subprocess, sys, termios, threading, time, pathlib

HERE = pathlib.Path(__file__).resolve().parent
CCXML = HERE.parent.parent / "cc2340r5/CC2340R5.ccxml"
DSLITE = "/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite"
OBJCOPY = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmobjcopy"
PORT = "/dev/cu.usbmodemLS470GTU1"
GPIO_DOUT, GPIO_DOE = 0x40023200, 0x40023500
APPS = ["empty", "gpiointerrupt", "i2ccontroller", "spicontroller", "spiperipheral"]

def sh(args, timeout=180):
    p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    return p.returncode, (p.stdout + p.stderr)

def read_mem(addr, length, out):
    # DSLite takes the length in 32-bit words, not bytes
    rc, log = sh([DSLITE, "memory", "-c", str(CCXML), "-r", f"{hex(addr)},{hex(length // 4)}", "-o", str(out)])
    if rc or not os.path.exists(out) or os.path.getsize(out) != length:
        raise RuntimeError(f"memory read {hex(addr)} failed: {log[-400:]}")
    return open(out, "rb").read()

def ihex(chunks):
    """Intel HEX for (address, bytes) chunks, with extended linear address records."""
    def rec(addr16, typ, data):
        b = bytes([len(data), addr16 >> 8, addr16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()
    lines, upper = [], None
    for base, data in chunks:
        for off in range(0, len(data), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16
                lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, data[off:off + 16]))
    return "\n".join(lines + [":00000001FF"]) + "\n"

def flash(image):
    t0 = time.time()
    rc, log = sh([DSLITE, "flash", "-c", str(CCXML), "-e", "-f", "-u", str(image)])
    return rc == 0, round(time.time() - t0, 1), log[-300:]

def uart_during(fn, secs):
    """Listen on the UART while fn() flashes and starts the app, then secs more. Returns (fn result, text)."""
    box = {}
    stop = threading.Event()
    def listen():
        box["text"] = uart(None, stop)
    t = threading.Thread(target=listen); t.start(); time.sleep(0.5)
    res = fn()
    time.sleep(secs); stop.set(); t.join()
    return res, box.get("text", "")

def uart(secs, stop=None):
    fd = os.open(PORT, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd)
    a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
    buf, end = b"", time.time() + (secs or 0)
    while (stop is not None and not stop.is_set()) or (stop is None and time.time() < end):
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            buf += os.read(fd, 4096)
    os.close(fd)
    return buf.decode("latin-1")

def main():
    imgs, bdir, out = map(pathlib.Path, sys.argv[1:4])
    secs = float(sys.argv[4]) if len(sys.argv) > 4 else 6
    bdir.mkdir(parents=True, exist_ok=True)
    main_bin, ccfg_bin, restore = bdir / "main_flash.bin", bdir / "ccfg.bin", bdir / "restore.hex"
    if not restore.exists():
        read_mem(0x0, 0x80000, main_bin)
        read_mem(0x4E020000, 0x800, ccfg_bin)
        if main_bin.read_bytes()[:8] in (b"\xff" * 8, b"\x00" * 8):
            raise SystemExit("backup looks blank; refusing to flash anything")
        restore.write_text(ihex([(0x0, main_bin.read_bytes()), (0x4E020000, ccfg_bin.read_bytes())]))
        print(f"backup: {restore}")
    try:
        for app in APPS:
            for variant in ("native", "ported"):
                img = imgs / f"{app}.{variant}.out"
                if not img.exists():
                    continue
                (ok, fsecs, flog), text = uart_during(lambda: flash(img), secs)
                regs = {}
                if ok:
                    try:
                        regs = {"dout": read_mem(GPIO_DOUT, 4, bdir / "r.bin").hex(), "doe": read_mem(GPIO_DOE, 4, bdir / "r.bin").hex()}
                    except RuntimeError as e:
                        regs = {"error": str(e)[:200]}
                rec = {"app": app, "variant": variant, "flashed": ok, "flash_secs": fsecs, "uart": text[-1500:], "gpio": regs,
                       "flash_log": None if ok else flog, "when": time.strftime("%Y-%m-%d %H:%M:%S")}
                open(out, "a").write(json.dumps(rec) + "\n")
                print(app, variant, "flashed" if ok else "FLASH FAILED", repr(text[:120]), regs)
    finally:
        ok, fsecs, flog = flash(restore)
        print("restore:", "ok" if ok else "FAILED " + flog, fsecs, "s")
        open(out, "a").write(json.dumps({"app": "restore", "flashed": ok, "flash_secs": fsecs, "flash_log": None if ok else flog}) + "\n")

if __name__ == "__main__":
    main()
