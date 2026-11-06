#!/usr/bin/env python3
"""Turn logs/bench-2026-10-24.txt and logs/userspace-2026-10-24.txt into result rows for data/results/am62l-evm.csv. Prints CSV lines; does not write."""
import csv, io, re, statistics, sys, collections
txt = open(sys.argv[1]).read(); app = open(sys.argv[2]).read()
rtt = collections.defaultdict(list); lz = collections.defaultdict(list); zl = collections.defaultdict(list)
for b in re.split(r'^=== ', txt, flags=re.M):
    m = re.match(r'rep (\d) zmode/lmode (\d) (\d)', b)
    if not m: continue
    z = 'poll' if m.group(2) == '1' else 'irq'; l = 'poll' if m.group(3) == '1' else 'irq'
    for sec in re.split(r'\n(?=rtt size|tput )', b):
        h = re.match(r'rtt size=(\d+)', sec)
        if h:
            for name in ('rtt', 'to-thread'):
                mm = re.search(r'^%s\s+median\s+(\d+)' % name, sec, re.M)
                rtt[(z, l, h.group(1), name)].append(int(mm.group(1)))
        t = re.match(r'tput (\S+) size=(\d+)', sec)
        if t:
            mm = re.search(r'(\d+) msgs/s\s+(\d+) kB/s', sec)
            d = lz if t.group(1) == 'linux>zephyr' else zl
            mode = z if d is lz else l
            d[(t.group(2), mode)].append((int(mm.group(1)), int(mm.group(2))))
COND = ("TMDS62LEVM, TI Linux SDK 12.01.00.05.03 kernel 6.18.38 from a USB stick, Linux on core 0 (maxcpus=1 mem=1536M cpuidle.off=1, performance governor 1.25 GHz), "
        "Zephyr a915b2f image of apps/core1-rpmsg on core 1 started by PSCI CPU_ON from the zrpmsg module, both in DDR; shared window 0xEFF00000 mapped non-cacheable on both sides; ")
rows = []
def add(test, variant, val, hi, runs, extra, note):
    rows.append(['am62l-evm', test, variant, 'measured', ('%g' % val), ('%g' % hi) if hi is not None else '', runs, 1250, COND + extra, '2026-10-24',
                 'docs/experiments/2026-10-24-am62l-linux-zephyr-link.md', 'by-us', note])
modes = {('irq', 'irq'): 'irq', ('poll', 'poll'): 'poll', ('irq', 'poll'): 'zirq-lpoll', ('poll', 'irq'): 'zpoll-lirq'}
for (z, l), tag in modes.items():
    for size in ('56', '496'):
        v = rtt[(z, l, size, 'rtt')]
        add('ipc_roundtrip_us', 'zrpmsg-%s-%sb' % (tag, size), statistics.median(v) / 1000, max(v) / 1000, len(v),
            'virtio-style vrings, RPMsg header, %s-byte payload echoed by Zephyr; Zephyr waits by %s, Linux by %s; in-kernel loop of 2,000 round trips after 64 warm-up, shared system counter (200 MHz); time from just before the doorbell to the Linux handler entry or poll hit' % (size, z, l),
            'median of three medians (value_hi is the largest); p99 about 2 to 5 times the median')
v = rtt[('irq', 'irq', '56', 'to-thread')]
add('ipc_roundtrip_us', 'zrpmsg-irq-56b-thread-wake', statistics.median(v) / 1000, max(v) / 1000, 3, 'same run as zrpmsg-irq-56b, timed until the sleeping kernel thread runs again', 'adds the scheduler wake to the doorbell round trip')
v = rtt[('irq', 'irq', '496', 'to-thread')]
add('ipc_roundtrip_us', 'zrpmsg-irq-496b-thread-wake', statistics.median(v) / 1000, max(v) / 1000, 3, 'same run as zrpmsg-irq-496b, timed until the sleeping kernel thread runs again', 'adds the scheduler wake to the doorbell round trip')
for size in ('56', '496'):
    v = [int(x) for x in re.findall(r'size=%s n=\d+ bad=0 median (\d+)' % size, app)]
    add('ipc_roundtrip_us', 'zrpmsg-char-userspace-%sb' % size, statistics.median(v) / 1000, max(v) / 1000, len(v),
        'userspace write then read on /dev/zrpmsg, 2,000 round trips after 50 warm-up, CLOCK_MONOTONIC, both sides by interrupt', 'median of three medians; includes two system calls and the thread wake')
for d, tests, dirn in ((lz, 'lz', 'linux-to-zephyr'), (zl, 'zl', 'zephyr-to-linux')):
    for (size, mode), v in sorted(d.items()):
        for test, idx, unit in (('ipc_throughput_kbps', 1, 'kB/s'), ('ipc_msgs_per_s', 0, 'msgs/s')):
            vals = [x[idx] for x in v]
            who = 'Zephyr' if dirn == 'linux-to-zephyr' else 'Linux'
            add(test, '%s-%sb-%s' % (dirn, size, mode), statistics.median(vals), None, len(vals),
                '%s sends 20,000 messages of %s payload bytes back to back, %s side waits by %s (the other side\'s mode does not enter; runs pooled over it); payload bytes, 1 kB = 1,000; first and last payload bytes checked' % ('Linux' if dirn == 'linux-to-zephyr' else 'Zephyr', size, who, mode),
                'median of %d runs; neither side copies the payload in the Linux-to-Zephyr run, so the rate is the per-message ring cost (about 1.5 us)' % len(vals) if dirn == 'linux-to-zephyr' else 'median of %d runs; Zephyr rings the Linux doorbell per message' % len(vals))
out = io.StringIO(); w = csv.writer(out, lineterminator='\n'); w.writerows(rows); sys.stdout.write(out.getvalue())
