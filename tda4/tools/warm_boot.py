#!/usr/bin/env python3
"""Warm-boot timer for the SK-TDA4VM over the Linux console. Sends `reboot`, logs the console with Mac
timestamps to warm-boot-<n>.log, and prints the marks: reboot sent, first 'U-Boot SPL' byte, 'Starting kernel',
first 'systemd[1]', 'login:'. Usage: warm_boot.py <n runs>"""
import serial, sys, time, re, datetime
N=int(sys.argv[1]) if len(sys.argv)>1 else 1
s=serial.Serial("/dev/cu.SLAB_USBtoUART1",115200,timeout=0)
def now(): return time.time()
def ts(t): return datetime.datetime.fromtimestamp(t).strftime('%H:%M:%S.%f')[:-3]
def drain(secs, stop=None):
    out=b""; t=now()
    while now()-t<secs:
        d=s.read(65536)
        if d: out+=d
        if stop and re.search(stop,out[-120:]): break
        time.sleep(0.02)
    return out
results=[]
for n in range(1,N+1):
    s.write(b"\x03"); time.sleep(0.3); s.write(b"\n"); r=drain(2.5, rb"# $|login: $")
    if b"login:" in r: s.write(b"root\n"); drain(5, rb"# $")
    s.write(b"sync\n"); drain(3, rb"# $")
    log=open(f"warm-boot-{n}.log","wb",buffering=0)
    marks={}
    t_cmd=now(); s.write(b"reboot\n"); marks['reboot_sent']=t_cmd
    log.write(f"### reboot sent {ts(t_cmd)}\n".encode())
    buf=b""; last=t_cmd
    while now()-t_cmd<180:
        d=s.read(65536)
        if d:
            t=now(); log.write(f"[{ts(t)}] ".encode()+d); buf+=d; last=t
            tail=buf[-4000:]
            if 'spl' not in marks and b"U-Boot SPL" in tail: marks['spl']=t
            if 'uboot' not in marks and re.search(rb"\nU-Boot 20", tail): marks['uboot']=t
            if 'kernel' not in marks and b"Starting kernel" in tail: marks['kernel']=t
            if 'systemd' not in marks and b"systemd[1]" in tail: marks['systemd']=t
            if 'spl' in marks and b"login:" in tail: marks['login']=t; break
        time.sleep(0.01)
    log.close()
    r={k:ts(v) for k,v in marks.items()}
    if 'spl' in marks and 'login' in marks:
        r['spl_to_login_s']=round(marks['login']-marks['spl'],2)
        r['cmd_to_login_s']=round(marks['login']-marks['reboot_sent'],2)
        r['cmd_to_spl_s']=round(marks['spl']-marks['reboot_sent'],2)
        for k in ('uboot','kernel','systemd'):
            if k in marks: r[k+'_s']=round(marks[k]-marks['spl'],2)
    print(f"run {n}: {r}", flush=True); results.append(r)
    time.sleep(25)   # let the login prompt settle and services finish
print("ALL", results)
