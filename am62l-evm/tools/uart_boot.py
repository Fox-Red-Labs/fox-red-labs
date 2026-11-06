#!/usr/bin/env python3
"""UART boot sender for the AM62L: XMODEM-1K to the boot ROM, YMODEM to U-Boot SPL.

  uart_boot.py PORT --tiboot3 F [--tispl F] [--uboot F] [--cmds FILE] [--log FILE] [--dry-run]
  uart_boot.py --selftest        (pty loopback against lrx/lrb from lrzsz; opens no board port)

Flow (SPRUJB4A 5.5.5; TI K3 UART boot guide): the ROM pings 'C' at 115200 8N1 on UART0 and takes
tiboot3.bin by XMODEM (CRC, 128 or 1024 byte blocks). SPL then pings 'C' again on the same UART
for tispl.bin (YMODEM; the ROM's stage 2 may instead ask for it by XMODEM, so the program picks by
whether SPL text came first) and again for u-boot.img (YMODEM). This program holds the port itself because on
macOS a baud set by `stty -f` reverts when stty closes the device (see capture.py). It prints every
non-protocol byte (the boot log), timestamps each stage, then optionally stops autoboot and feeds a
file of U-Boot commands one line at a time, waiting for the '=> ' prompt after each.
A lone 'C' (a burst of exactly one byte) is a ping; 'C' inside log text is not.
"""
import os, sys, time, select, termios, fcntl, struct, argparse, threading, subprocess, tempfile

SOH, STX, EOT, ACK, NAK, CAN, CRC_C = 1, 2, 4, 6, 21, 24, 0x43
IOSSIOSPEED = 0x80045402
T0 = time.time()
LOG = None
SAW_TEXT = False


def say(msg):
    line = f"[{time.time()-T0:8.3f}] {msg}"
    print(line, flush=True)
    if LOG:
        LOG.write(line + "\n"); LOG.flush()


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class Port:
    def __init__(self, path=None, fd=None, baud=115200):
        if fd is None:
            fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            a = termios.tcgetattr(fd)
            a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
            a[6][termios.VMIN] = 1; a[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, a)
            fcntl.ioctl(fd, IOSSIOSPEED, struct.pack('i', baud))
            termios.tcflush(fd, termios.TCIOFLUSH)
            fcntl.fcntl(fd, fcntl.F_SETFL, fcntl.fcntl(fd, fcntl.F_GETFL) & ~os.O_NONBLOCK)
        self.fd = fd

    def read(self, timeout):
        """Bytes that arrive until `timeout` seconds of silence; b'' if none."""
        out = b''
        while select.select([self.fd], [], [], timeout)[0]:
            d = os.read(self.fd, 4096)
            if not d:
                break
            out += d
            timeout = 0.03
        return out

    def write(self, data):
        while data:
            n = os.write(self.fd, data); data = data[n:]


def show(b):
    sys.stdout.write(b.decode('latin-1', 'replace').replace('\r', '')); sys.stdout.flush()
    if LOG:
        LOG.write(b.decode('latin-1', 'replace').replace('\r', '')); LOG.flush()


def wait_ping(p, what, timeout=330):
    """Wait for a lone 'C'. Everything else is boot log and is echoed."""
    global SAW_TEXT
    SAW_TEXT = False
    end = time.time() + timeout
    while time.time() < end:
        d = p.read(0.5)
        if d == b'C':
            say(f"ping from {what}"); return True
        if d:
            SAW_TEXT = True
            show(d)
    say(f"no ping from {what} in {timeout}s"); return False


def frame(blk, data, size):
    data = data.ljust(size, b'\x1a')
    head = bytes([STX if size == 1024 else SOH, blk & 255, 255 - (blk & 255)])
    return head + data + struct.pack('>H', crc16(data))


def send_block(p, pkt, tries=10):
    for _ in range(tries):
        p.write(pkt)
        while True:
            d = p.read(3)
            if not d:
                break
            r = d[-1:]
            if bytes([ACK]) in d:
                return d
            if r == bytes([NAK]) or r == b'C':
                break
            if r == bytes([CAN]):
                raise RuntimeError("receiver cancelled")
            show(d)
    raise RuntimeError("block not acknowledged")


def send_eot(p):
    for _ in range(10):
        p.write(bytes([EOT]))
        d = p.read(3)
        if bytes([ACK]) in d:
            return
        # YMODEM receivers may NAK the first EOT; send it again
    raise RuntimeError("EOT not acknowledged")


def xmodem(p, data, label):
    say(f"XMODEM {label}: {len(data)} bytes, {(len(data)+1023)//1024} blocks")
    for i in range(0, len(data), 1024):
        send_block(p, frame(i // 1024 + 1, data[i:i + 1024], 1024))
    send_eot(p)
    say(f"XMODEM {label} done")


def ymodem(p, data, name):
    say(f"YMODEM {name}: {len(data)} bytes")
    hdr = f"{name}\0{len(data)}\0".encode()
    d = send_block(p, frame(0, hdr, 128))
    if b'C' not in d and not wait_ping(p, "receiver (data phase)", 10):
        raise RuntimeError("no 'C' after header")
    for i in range(0, len(data), 1024):
        send_block(p, frame(i // 1024 + 1, data[i:i + 1024], 1024))
    send_eot(p)
    # end of batch: receiver pings once more, then we send the empty block 0
    d = p.read(2)
    if d:
        show(d.replace(b'C', b'')) if d != b'C' else None
    try:
        send_block(p, frame(0, b'', 128), tries=2)
    except RuntimeError:
        pass  # SPL may already have started the next stage; not an error
    say(f"YMODEM {name} done")


def prompt_wait(p, timeout):
    buf = b''
    end = time.time() + timeout
    while time.time() < end:
        d = p.read(0.3)
        if d:
            show(d); buf += d
            if buf.rstrip(b' ').endswith(b'=>') or buf.endswith(b'=> '):
                return True
        else:
            # idle: if we were mid-autoboot countdown, poke it
            p.write(b' ')
    return False


def run_cmds(p, path):
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        say(f"U-Boot> {line}")
        p.write(line.encode() + b'\r')
        if not prompt_wait(p, 600):
            say("no prompt back; stopping"); return False
    return True


def selftest():
    import pty
    res = []
    for kind, exe, a in (("xmodem", "lrx", ["-c"]), ("ymodem", "lrb", [])):
        m, s = pty.openpty()
        d = tempfile.mkdtemp()
        data = os.urandom(5000)
        extra = ["out.bin"] if kind == "xmodem" else []
        tool = subprocess.Popen([exe, "-q"] + a + extra, stdin=s, stdout=s, cwd=d)
        p = Port(fd=m)
        time.sleep(0.5)
        try:
            if kind == "xmodem":
                if not wait_ping(p, "lrx", 20):
                    raise RuntimeError("no ping")
                xmodem(p, data, "selftest")
                got = open(os.path.join(d, "out.bin"), 'rb').read()[:len(data)]
            else:
                if not wait_ping(p, "lrb", 20):
                    raise RuntimeError("no ping")
                ymodem(p, data, "t.bin")
                got = open(os.path.join(d, "t.bin"), 'rb').read()
            res.append((kind, got == data))
        except Exception as e:
            res.append((kind, f"FAIL {e}"))
        tool.kill()
    for r in res:
        print("selftest", *r)
    return all(r[1] is True for r in res)


def main():
    global LOG
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs='?')
    ap.add_argument("--tiboot3"); ap.add_argument("--tispl"); ap.add_argument("--uboot")
    ap.add_argument("--cmds", help="file of U-Boot commands to run once the prompt appears")
    ap.add_argument("--tispl-proto", choices=["auto", "x", "y"], default="auto",
                    help="auto: YMODEM if SPL printed text before pinging, else XMODEM (ROM stage 2)")
    ap.add_argument("--listen", type=float, default=0, help="after the last file, keep reading UART A for this many seconds and log it with times (E200: the payload's own output)")
    ap.add_argument("--log"); ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(0 if selftest() else 1)
    if not (a.port and a.tiboot3):
        ap.error("port and --tiboot3 are required")
    files = [("tiboot3.bin", a.tiboot3), ("tispl.bin", a.tispl), ("u-boot.img", a.uboot)]
    for n, f in files:
        if f:
            print(f"{n}: {f} {os.path.getsize(f)} bytes")
    if a.dry_run:
        print("dry run: port not opened"); return
    if a.log:
        LOG = open(a.log, 'a')
    p = Port(a.port)
    say(f"port {a.port} open at 115200 8N1; power cycle the board now")
    if not wait_ping(p, "ROM"):
        sys.exit(2)
    xmodem(p, open(a.tiboot3, 'rb').read(), "tiboot3.bin")
    if a.listen and not a.tispl:
        end = time.time() + a.listen
        while time.time() < end:
            d = p.read(0.5)
            if d:
                say("rx %d bytes: %r" % (len(d), d[:200]))
                show(d)
        say(f"listened {a.listen} s after tiboot3.bin")
        return
    for name, f in files[1:]:
        if not f:
            continue
        if not wait_ping(p, f"SPL for {name}"):
            sys.exit(3)
        proto = "y"
        if name == "tispl.bin":
            proto = a.tispl_proto if a.tispl_proto != "auto" else ("y" if SAW_TEXT else "x")
        say(f"{name}: protocol {'YMODEM' if proto == 'y' else 'XMODEM'} (text before ping: {SAW_TEXT})")
        data = open(f, 'rb').read()
        ymodem(p, data, name) if proto == "y" else xmodem(p, data, name)
    if a.uboot:
        say("waiting for the U-Boot prompt (sending spaces to stop autoboot)")
        if prompt_wait(p, 120):
            say("U-Boot prompt")
            if a.cmds:
                run_cmds(p, a.cmds)
        else:
            say("no prompt"); sys.exit(4)


if __name__ == "__main__":
    main()
