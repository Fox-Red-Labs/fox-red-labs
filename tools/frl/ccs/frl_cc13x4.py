"""frl's debug steps for a CC13x4 or CC26x4 (LP-CC1354P10-1) through CCStudio's Python scripting.

Run with CCStudio's own Python (tools/frl/ccs13.py does this):
  <ccs>/ccs/scripting/python/run.sh frl_cc13x4.py <target.ccxml> <op> [args]

  identity                          ID <ICEPICK_DEVICE_ID hex> <USER_ID hex>
  backup  <addr> <bytes> <out.bin>  OK saved
  program <bin> <addr> [<bin> <addr> ...]
                                    loadBinaryProgram, then verifyBinaryProgram, each block at its own address,
                                    in one session: OK programmed
  verify  <bin> <addr> [...]        OK verified | MISMATCH
  restore <bin> <addr> [...]        as program: OK restored
  start                             start the program in flash from its vector table, leave it running: OK started
  sample  <n addr> <log addr> <len> <seconds> <dio>
                                    start the program, let it run, halt, read the toggle log: N <count> <span ms>,
                                    then one "T <ms>" per logged toggle, then E and O: that DIO's own output-enable
                                    and output bits (0, 1, or ? when the register could not be read)

The chip accepts a debugger in the first seconds after power-on, and after some programs (a radio demo, standby)
it refuses a new connect (Error -242) until its power is cycled. A refused connect exits 3; tools/frl/ccs13.py
decides whether a power cycle is safe (power_cycle) and runs the step again. Never switch power while connected
(Error -261).
"""
import os
import re
import sys
import time

ICEPICK_DEVICE_ID, USER_ID = 0x50000B18, 0x50000A94      # FCFG1_BASE 0x50000800 + 0x318 and + 0x294 (hw_fcfg1.h, F2 SDK 8.33)
GPIO_DOE = 0x400220D0                                    # GPIO_BASE 0x40022000 + GPIO_O_DOE31_0 (hw_gpio.h)
GPIO_DOUT = 0x40022080                                   # GPIO_BASE + GPIO_O_DOUT31_0 (hw_gpio.h)


def say(*a):
    print(*a, flush=True)


def connect(s):
    """One connect. A refusal exits 3; ccs13.py, which sees CCStudio's own log line with the error number, decides
    whether a power cycle is safe and runs this step again."""
    try:
        s.target.connect()
    except Exception as e:                                # noqa: BLE001 (CCStudio raises its own types)
        say("fatal: connect refused: " + (str(e).strip().splitlines()[-1][:160] if str(e).strip() else type(e).__name__))
        sys.exit(3)


def start(s):
    """Run the program in flash from reset state: SP and PC from its vector table at 0 (System Reset is not offered
    on this chip through this probe, and a board reset drops the connection)."""
    sp, pc = s.memory.readOne(0x0), s.memory.readOne(0x4)
    s.registers.write("SP", sp)
    s.registers.write("PC", pc & ~1)
    s.target.run(False)


def pin_lines(read, dio):
    """E and O lines: this DIO's own bit of the output-enable and output registers, or ? when a read fails."""
    out = []
    for tag, addr in (("E", GPIO_DOE), ("O", GPIO_DOUT)):
        try:
            out.append("%s %d" % (tag, (read(addr) >> dio) & 1))
        except Exception:                                    # noqa: BLE001 (peripheral domain can be off)
            out.append(tag + " ?")
    return out


def pairs(args):
    if not args or len(args) % 2:
        say("fatal: give <bin> <addr> pairs")
        sys.exit(2)
    return [(args[k], int(args[k + 1], 0)) for k in range(0, len(args), 2)]


def main():
    from scripting import initScripting                    # CCStudio's own Python only; tests import pin_lines without it
    ccxml, op, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    ds = initScripting()
    ds.setScriptingTimeout(300000)
    ds.configure(ccxml)
    s = ds.openSession(re.compile(".*cortex.*", re.IGNORECASE))
    connect(s)
    s.target.halt()
    leave_running = False
    try:
        if op == "identity":
            say("ID %08X %08X" % (s.memory.readOne(ICEPICK_DEVICE_ID), s.memory.readOne(USER_ID)))
        elif op == "backup":
            addr, n, out = int(args[0], 0), int(args[1], 0), args[2]
            s.memory.saveBinary(addr, n // 4, out)             # count is in 32-bit words
            say("OK saved %d bytes" % os.path.getsize(out))
        elif op in ("program", "restore"):
            for img, addr in pairs(args):
                s.memory.loadBinaryProgram(img, addr)        # erases the 2 KB sectors the block covers, writes it
                s.memory.verifyBinaryProgram(img, addr)      # raises on a difference
            start(s)                                         # the new program runs from its own vector table
            leave_running = True
            say("OK programmed" if op == "program" else "OK restored")
        elif op == "verify":
            try:
                for img, addr in pairs(args):
                    s.memory.verifyBinaryProgram(img, addr)
                say("OK verified")
            except Exception as e:                           # noqa: BLE001
                say("MISMATCH " + str(e).strip().splitlines()[-1][:160])
        elif op == "start":
            start(s)
            leave_running = True
            say("OK started")
        elif op == "sample":
            n_addr, log_addr, length, secs, dio = int(args[0], 0), int(args[1], 0), int(args[2], 0), float(args[3]), int(args[4])
            start(s)
            t0 = time.time()
            time.sleep(secs)
            s.target.halt()
            span = round((time.time() - t0) * 1000)
            n = s.memory.readOne(n_addr)
            log = s.memory.read(log_addr, length, 32)
            first = max(0, n - length)
            say("N %d %d" % (n, span))
            for k in range(first, n):
                say("T %d" % (log[k % length] & 0xFFFFFFFF))
            for line in pin_lines(s.memory.readOne, dio):
                say(line)
            s.target.run(False)
            leave_running = True
        else:
            say("fatal: unknown op " + op)
            sys.exit(2)
    except SystemExit:
        raise
    except Exception as e:                                   # noqa: BLE001
        say("fatal: " + (str(e).strip().splitlines()[-1][:200] if str(e).strip() else type(e).__name__))
        sys.exit(1)
    finally:
        if not leave_running:
            try:
                s.target.run(False)                          # a halted chip after frl is a surprise; let it run
            except Exception:                                # noqa: BLE001
                pass
        s.target.disconnect()
        ds.shutdown()


if __name__ == "__main__":
    main()
