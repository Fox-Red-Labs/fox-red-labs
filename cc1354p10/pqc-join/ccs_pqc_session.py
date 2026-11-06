"""One CCStudio scripting session for the post-quantum join on the LP-CC1354P10-1 (probe LS45013A).
Run with CCStudio's Python:
  run.sh ccs_pqc_session.py <ccxml> <pqc image.out> <restore image.out> <run dir> [max seconds]
Steps: connect (retrying: the debugger connects only just after a power-on boot), reset and halt, back up flash (0 to 1 MB) and the CCFG
page (0x50000000, 1 kB) to <run dir> (outside the repo), load and start the pqc image, write <run dir>/running, wait for
<run dir>/stop (or the time limit), halt, load the restore image, read flash and CCFG back and compare with the backup, start the
restored image, write <run dir>/restore-report.txt. The restore always runs, also after an error."""
import hashlib, re, struct, sys, time
from pathlib import Path
from scripting import initScripting

ccxml, pqc_img, restore_img, rd = sys.argv[1], sys.argv[2], sys.argv[3], Path(sys.argv[4])
limit = float(sys.argv[5]) if len(sys.argv) > 5 else 900
RANGES = [(0x0, 0x100000), (0x50000000, 0x400)]
rd.mkdir(parents=True, exist_ok=True)
log = open(rd / "session.log", "a")

def say(*a):
    m = time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a)
    print(m, flush=True); log.write(m + "\n"); log.flush()

def ihex(chunks):
    def rec(a16, typ, data):
        b = bytes([len(data), a16 >> 8, a16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()
    lines, upper = [], None
    for base, data in chunks:
        for off in range(0, len(data), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16; lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, data[off:off + 16]))
    return "\n".join(lines + [":00000001FF"]) + "\n"

def read_bytes(s, start, length):
    out = bytearray()
    for a in range(start, start + length, 4096):
        n = min(1024, (start + length - a) // 4)
        out += b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in s.memory.read(a, n))
    return bytes(out)

ds = initScripting(); ds.setScriptingTimeout(300000)
ds.configure(ccxml)
s = None
for attempt in range(60):
    try:
        if s is None:
            s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
        s.target.connect()
        break
    except Exception as e:
        say("connect failed (%d/60): %s" % (attempt + 1, str(e)[-60:]))
        if "-260" in str(e):
            s = None
        if attempt == 59:
            raise
say("connected")
backup = []
try:
    try:
        s.target.reset()
        say("reset and halted")
    except Exception as e:                       # "System Reset is not currently allowed" on CC13x4: halt instead
        say("reset refused (%s); halting" % str(e)[-50:])
        s.target.halt()
        say("halted")
    t0 = time.time()
    for start, length in RANGES:
        data = read_bytes(s, start, length)
        (rd / ("backup_%#x.bin" % start)).write_bytes(data)
        backup.append((start, data))
        say("backup %#x +%d sha256 %s" % (start, length, hashlib.sha256(data).hexdigest()))
    say("backup took %.0f s" % (time.time() - t0))
    if backup[0][1][:8] in (b"\xff" * 8, b"\x00" * 8):
        raise RuntimeError("backup looks blank: refusing to load anything")
    s.memory.loadProgram(pqc_img)
    say("loaded", pqc_img)
    s.target.run(False)
    say("running")
    (rd / "running").write_text("1\n")
    t0 = time.time()
    peeked = False
    while not (rd / "stop").exists() and time.time() - t0 < limit:
        if (rd / "peek").exists() and not peeked:      # debugging aid: where is the CPU?
            peeked = True
            s.target.halt()
            for r in ("PC", "LR", "SP"):
                try:
                    say("peek %s = %#x" % (r, s.registers.read(r)))
                except Exception as e:
                    say("peek %s failed %s" % (r, str(e)[-60:]))
            s.target.run(False)
        time.sleep(1)
    say("stop requested" if (rd / "stop").exists() else "time limit")
    s.target.halt()
except Exception as e:
    say("ERROR during run:", str(e)[-300:])
finally:
    report = []
    try:
        if not backup:
            report.append("no backup was taken; flash not touched by this script")
        else:
            try:
                s.target.halt()
            except Exception:
                pass
            s.memory.loadProgram(restore_img)
            say("loaded restore image", restore_img)
            same = True
            for start, data in backup:
                now = read_bytes(s, start, len(data))
                (rd / ("readback_%#x.bin" % start)).write_bytes(now)
                diff = [i for i in range(len(data)) if now[i] != data[i]]
                report.append("%#x +%d: %s (%d bytes differ)" % (start, len(data), "identical" if not diff else "DIFFERENT", len(diff)))
                if diff:
                    same = False
                    report.append("  first differences at offsets: " + ", ".join("%#x" % d for d in diff[:8]))
            if not same:                          # the loader does not restore flash the image does not carry (runtime data): write it back from the backup
                rb = (rd / "readback_0x0.bin").read_bytes()
                sect = sorted({i // 8192 for i in range(len(rb)) if rb[i] != backup[0][1][i]})
                chunks = [(k * 8192, backup[0][1][k * 8192:(k + 1) * 8192]) for k in sect]
                (rd / "fix.hex").write_text(ihex(chunks))
                s.memory.loadProgram(str(rd / "fix.hex"))
                report.append("rewrote 8 kB sectors from the backup: " + ", ".join("%#x" % (k * 8192) for k in sect))
                same = True
                for start, data in backup:
                    now = read_bytes(s, start, len(data))
                    (rd / ("readback_%#x.bin" % start)).write_bytes(now)
                    n = sum(1 for i in range(len(data)) if now[i] != data[i])
                    report.append("after rewrite %#x +%d: %d bytes differ" % (start, len(data), n))
                    same = same and n == 0
            report.append("read-back sha256 flash %s" % hashlib.sha256((rd / "readback_0x0.bin").read_bytes()).hexdigest())
            s.target.run(False)
            report.append("restored image started")
            report.append("READBACK " + ("MATCHES BACKUP" if same else "DIFFERS FROM BACKUP"))
    except Exception as e:
        report.append("RESTORE ERROR: " + str(e)[-300:])
    (rd / "restore-report.txt").write_text("\n".join(report) + "\n")
    say("\n".join(report))
    try:
        s.target.disconnect(); ds.shutdown()
    except Exception:
        pass
