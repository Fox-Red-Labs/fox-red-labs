#!/usr/bin/env python3
"""On the board: CPU idle and memory over N seconds, one line.
  sample.py LABEL SECONDS
Memory used = MemTotal - MemAvailable (MB). CPU idle = idle+iowait share of /proc/stat jiffies over the window, both cores."""
import sys, time

def stat():
    v = list(map(int, open('/proc/stat').readline().split()[1:]))
    return sum(v), v[3] + v[4]

def mem():
    d = {l.split(':')[0]: int(l.split()[1]) for l in open('/proc/meminfo')}
    return (d['MemTotal'] - d['MemAvailable']) / 1024, d['MemAvailable'] / 1024

label, secs = sys.argv[1], float(sys.argv[2])
t0, i0 = stat(); time.sleep(secs); t1, i1 = stat()
used, avail = mem()
print(f"SAMPLE {label} idle_pct={100*(i1-i0)/(t1-t0):.1f} mem_used_mb={used:.0f} mem_avail_mb={avail:.0f} load={open('/proc/loadavg').read().split()[0]}")
