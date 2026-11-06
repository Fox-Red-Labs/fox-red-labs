#!/usr/bin/env python3
"""Decode a captured TI binary log (raw serial bytes saved by our runners as '<time> <bytes repr>' lines)
by replaying it through a pseudo-terminal into TI's tilogger.
Usage: replay_tilog.py <uart.log> <elf .out> [tilogger path]"""
import ast, os, pty, subprocess, sys, time, threading
log, elf = sys.argv[1], sys.argv[2]
tl = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser("~/.venvs/tilogger/bin/tilogger")
data = b"".join(ast.literal_eval(l.split(" ", 1)[1].strip()) for l in open(log))
master, slave = pty.openpty()
name = os.ttyname(slave)
p = subprocess.Popen([tl, "--elf", elf, "uart", name, "115200", "stdout"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     env=dict(os.environ, PYTHONUNBUFFERED="1"))
out = []
threading.Thread(target=lambda: out.extend(iter(p.stdout.readline, b"")), daemon=True).start()
time.sleep(3)
for i in range(0, len(data), 512):
    os.write(master, data[i:i + 512]); time.sleep(0.01)
time.sleep(4); p.terminate(); time.sleep(1)
sys.stdout.write(b"".join(out).decode("latin-1"))
