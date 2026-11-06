#!/usr/bin/env python3
"""Decode a run.sh result for the bench image. TIMG0 runs at half the CPU clock on this chip at 80 MHz
(proved by the image: TIMG0 counts vs SysTick cycles), so cycles = counts x ratio.  decode.py <file.mdw.txt> [cpu_mhz]"""
import re,sys
w=[]
fault=None
for l in open(sys.argv[1]):
    a,rest=l.split(':'); v=[int(x,16) for x in rest.split()]
    if a.strip()=='0x20200710' or len(v)==4 and fault is None and a.strip().endswith('710'): fault=v
    else: w+=v
mhz=float(sys.argv[2]) if len(sys.argv)>2 else 80
m,n,timg,sys_,tread=w[:5]; r=sys_/timg
print(f"magic {m:#x} samples {n} timg {timg} systick {sys_} ratio {r:.3f} timer_read {tread} counts; fault {fault}")
for i,t in enumerate(['yield_rt','sem_wake','notify_wake','queue_rt','irq_entry','irq_sem_task','irq_notify_task']):
    b,wo,wn,s=w[5+4*i:9+4*i]
    print(f"{t:16} best {b*r:7.0f} worst {wo*r:7.0f} worst_notick {wn*r:7.0f} mean {s/n*r:8.1f} cycles; best {b*r/mhz:.3f} us mean {s/n*r/mhz:.3f} us")
print("errors",w[33],"ticks_end",w[42],"stack",w[34:38],"unused",w[38:42])
