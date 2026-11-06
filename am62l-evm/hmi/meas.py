#!/usr/bin/env python3
"""meas.py LABEL SECS CMD... : run CMD, warm up 5 s, then sample CPU (percent of both cores) and RSS over SECS.
Writes stderr/stdout of the command to LABEL.log. Prints one summary line."""
import subprocess, sys, time, os, signal
label, secs = sys.argv[1], int(sys.argv[2]); cmd = sys.argv[3:]
hz = os.sysconf('SC_CLK_TCK')
def ticks(pid):
    f = open(f'/proc/{pid}/stat').read().rsplit(')',1)[1].split()
    return int(f[11]) + int(f[12])
def total():
    v = open('/proc/stat').readline().split()[1:]; return sum(map(int,v)), int(v[3])
env = dict(os.environ)
log = open(label + '.log', 'w')
la = open('/proc/loadavg').read().split()[0]
mem = [l for l in open('/proc/meminfo') if l.startswith('MemAvailable')][0].split()[1]
p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT, env=env)
time.sleep(5)
tp = p.pid
want = os.environ.get('PROC')
if want:
    for d in os.listdir('/proc'):
        if d.isdigit():
            try:
                if open(f'/proc/{d}/comm').read().strip() == want: tp = int(d)
            except Exception: pass
t0, i0 = total(); c0 = ticks(tp); w0 = time.time()
time.sleep(secs)
t1, i1 = total(); c1 = ticks(tp); w1 = time.time()
st = open(f'/proc/{tp}/status').read()
rss = [l for l in st.splitlines() if l.startswith('VmRSS')][0].split()[1]
hwm = [l for l in st.splitlines() if l.startswith('VmHWM')][0].split()[1]
cpu_proc = (c1-c0)/hz/(w1-w0)*100/2     # percent of the two-core total
cpu_sys = (1-(i1-i0)/(t1-t0))*100       # whole-system busy percent
import datetime
f=lambda t: datetime.datetime.fromtimestamp(t,datetime.UTC).replace(tzinfo=None).strftime('%H:%M:%S.%f')
os.system('kill -INT %d 2>/dev/null' % tp); time.sleep(1); p.kill(); p.wait()
print(f'{label} win={f(w0)}..{f(w1)} load_before={la} memavail_kb={mem} proc_cpu_of_2cores={cpu_proc:.1f}% system_busy={cpu_sys:.1f}% rss_kb={rss} hwm_kb={hwm}')
